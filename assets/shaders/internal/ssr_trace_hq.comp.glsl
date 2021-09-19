#version 310 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "ssr_trace_hq_interface.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
	UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

#define Z_THICKNESS 0.035
#define MAX_STEPS 256
#define MOST_DETAILED_MIP 0
#define LEAST_DETAILED_MIP 5

#define FLOAT_MAX 3.402823466e+38

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D depth_texture;
layout(binding = NORM_TEX_SLOT) uniform highp sampler2D norm_texture;

layout(binding = OUTPUT_TEX_SLOT, rgb10_a2) uniform writeonly image2D output_texture;

//
// https://github.com/GPUOpen-Effects/FidelityFX-SSSR
//

float IntersectRay(vec3 ray_origin_ss, vec3 ray_origin_vs, vec3 ray_dir_vs, out vec3 out_hit_point) {
    vec4 ray_offsetet_ss = shrd_data.uProjMatrix * vec4(ray_origin_vs + ray_dir_vs, 1.0);
	ray_offsetet_ss.xyz /= ray_offsetet_ss.w;
	
#if defined(VULKAN)
	ray_offsetet_ss.y = -ray_offsetet_ss.y;
	ray_offsetet_ss.xy = 0.5 * ray_offsetet_ss.xy + 0.5;
#else // VULKAN
	ray_offsetet_ss.xyz = 0.5 * ray_offsetet_ss.xyz + 0.5;
#endif // VULKAN
	
	vec3 ray_dir_ss = normalize(ray_offsetet_ss.xyz - ray_origin_ss);
	vec3 ray_dir_ss_inv = mix(1.0 / ray_dir_ss, vec3(FLOAT_MAX), equal(ray_dir_ss, vec3(0.0)));
	
	int cur_mip = MOST_DETAILED_MIP;
	
	vec2 cur_mip_res = shrd_data.uResAndFRes.xy / exp2(MOST_DETAILED_MIP);
	vec2 cur_mip_res_inv = 1.0 / cur_mip_res;
	
	vec2 uv_offset = 0.005 * exp2(MOST_DETAILED_MIP) / shrd_data.uResAndFRes.xy;
	uv_offset = mix(uv_offset, -uv_offset, lessThan(ray_dir_ss.xy, vec2(0.0)));
	
	vec2 floor_offset = mix(vec2(1.0), vec2(0.0), lessThan(ray_dir_ss.xy, vec2(0.0)));
	
	float cur_t;
	vec3 cur_pos_ss;
	
	{ // advance ray to avoid self intersection
		vec2 cur_mip_pos = cur_mip_res * ray_origin_ss.xy;
		
		vec2 xy_plane = floor(cur_mip_pos) + floor_offset;
		xy_plane = xy_plane * cur_mip_res_inv + uv_offset;
		
		vec2 t = (xy_plane - ray_origin_ss.xy) * ray_dir_ss_inv.xy;
		cur_t = min(t.x, t.y);
		cur_pos_ss = ray_origin_ss.xyz + cur_t * ray_dir_ss;
	}

	int iter = 0;
    while (iter++ < MAX_STEPS && cur_mip >= MOST_DETAILED_MIP) {
		vec2 cur_pos_px = cur_mip_res * cur_pos_ss.xy;
		float surf_z = texelFetch(depth_texture, clamp(ivec2(cur_pos_px), ivec2(0), ivec2(cur_mip_res - 1)), cur_mip).r;
		bool increment_mip = cur_mip < LEAST_DETAILED_MIP;
		
		{ // advance ray
			vec2 xy_plane = floor(cur_pos_px) + floor_offset;
			xy_plane = xy_plane * cur_mip_res_inv + uv_offset;
			vec3 boundary_planes = vec3(xy_plane, surf_z);
			// o + d * t = p' => t = (p' - o) / d
			vec3 t = (boundary_planes - ray_origin_ss.xyz) * ray_dir_ss_inv;
			
			t.z = (ray_dir_ss.z > 0.0) ? t.z : FLOAT_MAX;
			
			// choose nearest intersection
			float t_min = min(min(t.x, t.y), t.z);
			
			bool is_above_surface = surf_z > cur_pos_ss.z;
			
			increment_mip = increment_mip && (t_min != t.z) && is_above_surface;
			
			cur_t = is_above_surface ? t_min : cur_t;
			cur_pos_ss = ray_origin_ss.xyz + cur_t * ray_dir_ss;
		}
		
		cur_mip += increment_mip ? 1 : -1;
		cur_mip_res *= increment_mip ? 0.5 : 2.0;
		cur_mip_res_inv *= increment_mip ? 2.0 : 0.5;
	}
	
	if (iter > MAX_STEPS) {
		// Intersection was not found
		return 0.0;
	}
	
	// Reject out-of-view hits
	if (any(lessThan(cur_pos_ss.xy, vec2(0.0))) || any(greaterThan(cur_pos_ss.xy, vec2(1.0)))) {
		return 0.0;
	}
	
	// Reject if we hit surface from the back
	vec3 hit_normal_fetch = textureLod(norm_texture, cur_pos_ss.xy, 0.0).xyz;
	vec3 hit_normal_ws = 2.0 * hit_normal_fetch - 1.0;
    vec3 hit_normal_vs = (shrd_data.uViewMatrix * vec4(hit_normal_ws, 0.0)).xyz;
	if (dot(hit_normal_vs, ray_dir_vs) > 0.0) {
		return 0.0;
	}
	
	vec3 hit_point_cs = cur_pos_ss;
#if defined(VULKAN)
    hit_point_cs.xy = 2.0 * hit_point_cs.xy - 1.0;
    hit_point_cs.y = -hit_point_cs.y;
#else // VULKAN
    hit_point_cs.xyz = 2.0 * hit_point_cs.xyz - 1.0;
#endif // VULKAN
	
	vec4 hit_point_vs = shrd_data.uInvProjMatrix * vec4(hit_point_cs, 1.0);
	hit_point_vs.xyz /= hit_point_vs.w;
	
	out_hit_point = hit_point_vs.xyz;

	float hit_depth_fetch = texelFetch(depth_texture, ivec2(cur_pos_ss.xy * params.resolution.xy), 0).r;
	vec4 hit_surf_cs = vec4(hit_point_cs.xy, hit_depth_fetch, 1.0);
#if !defined(VULKAN)
    hit_surf_cs.z = 2.0 * hit_surf_cs.z - 1.0;
#endif // VULKAN
	
	vec4 hit_surf_vs = shrd_data.uInvProjMatrix * hit_surf_cs;
	hit_surf_vs.xyz /= hit_surf_vs.w;
	float dist_vs = distance(hit_point_vs.xyz, hit_surf_vs.xyz);
	
	float confidence = clamp(1.0 - smoothstep(0.0, Z_THICKNESS, dist_vs), 0.0, 1.0);
	confidence *= confidence;
	
	return confidence;
}

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
	if (!all(lessThan(gl_GlobalInvocationID.xy, params.resolution.xy))) return;

    ivec2 pix_uvs = ivec2(gl_GlobalInvocationID.xy);
    vec2 norm_uvs = (vec2(pix_uvs) + 0.5) / shrd_data.uResAndFRes.xy;

    vec4 normal_tex = texelFetch(norm_texture, pix_uvs, 0);
    if (normal_tex.w < 0.0001) {
        imageStore(output_texture, pix_uvs, vec4(0.0));
        return;
    }

	float depth = texelFetch(depth_texture, pix_uvs, 0).r;

    vec3 normal_ws = 2.0 * normal_tex.xyz - 1.0;
    vec3 normal_vs = normalize((shrd_data.uViewMatrix * vec4(normal_ws, 0.0)).xyz);

	vec3 ray_origin_ss = vec3(norm_uvs, depth);
    vec4 ray_origin_cs = vec4(ray_origin_ss, 1.0);
#if defined(VULKAN)
    ray_origin_cs.xy = 2.0 * ray_origin_cs.xy - 1.0;
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    ray_origin_cs.xyz = 2.0 * ray_origin_cs.xyz - 1.0;
#endif // VULKAN

    vec4 ray_origin_vs = shrd_data.uInvProjMatrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec3 refl_ray_vs = reflect(view_ray_vs, normal_vs); 

    vec3 hit_point;
    vec3 out_color = vec3(0.0);
	float hit_confidence = IntersectRay(ray_origin_ss, ray_origin_vs.xyz, refl_ray_vs, hit_point);
    if (hit_confidence > 0.0) {
        // reproject hitpoint into a clip space of previous frame
        vec4 hit_prev = shrd_data.uDeltaMatrix * vec4(hit_point, 1.0);
#if defined(VULKAN)
        hit_prev.y = -hit_prev.y;
#endif // VULKAN
        hit_prev /= hit_prev.w;
        hit_prev.xy = 0.5 * hit_prev.xy + 0.5;

		// Fade out based on how close border is
		vec2 fov = 0.05 * vec2(shrd_data.uResAndFRes.y / shrd_data.uResAndFRes.x, 1.0);
		vec2 border = smoothstep(vec2(0.0), fov, hit_prev.xy) * (1.0 - smoothstep(1.0 - fov, vec2(1.0), hit_prev.xy));
		float vignette = border.x * border.y;

		out_color.rg = hit_prev.xy;
		out_color.b = hit_confidence * vignette;
    }
    
    imageStore(output_texture, pix_uvs, vec4(out_color, 0.0));
}

