#version 430 core

#include "_cs_common.glsl"
#include "gtao_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;

layout(binding = OUT_IMG_SLOT, r8) uniform image2D g_out_img;

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

// Mostly taken from: https://github.com/asylum2010/Asylum_Tutorials/blob/4e97019dbce70c8ee3d9d3ed555fd2e3189ffef6/ShaderTutors/media/shadersGL/gtao.frag

float Falloff(const float dist2) {
    const float FALLOFF_START2 = 0.3;
    const float FALLOFF_END2 = 0.8;
	return 2.0 * clamp((dist2 - FALLOFF_START2) / (FALLOFF_END2 - FALLOFF_START2), 0.0, 1.0);
}

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }
    const float center_depth = texelFetch(g_depth_tex, ivec2(gl_GlobalInvocationID.xy), 0).x;
    if (center_depth == 0.0) {
        imageStore(g_out_img, ivec2(gl_GlobalInvocationID.xy), vec4(1.0));
        return;
    }
    const float center_depth_lin = LinearizeDepth(center_depth, g_params.clip_info);

    const vec2 pix_uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(g_params.img_size);
    const vec3 center_point_vs = ReconstructViewPosition(pix_uv, g_params.frustum_info, -center_depth_lin, 0.0 /* is_ortho */);
    const vec3 view_dir_vs = normalize(-center_point_vs);

    const vec3 center_normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(gl_GlobalInvocationID.xy), 0).x).xyz;
    vec3 center_normal_vs = normalize((g_params.view_from_world * vec4(center_normal_ws, 0.0)).xyz);
#if defined(VULKAN)
    center_normal_vs.y = -center_normal_vs.y;
#endif

    const int StepsCount = 4;
    const int DirectionsCount = 1;

    const float radius = min(48.0, 128.0 / abs(center_point_vs.z));
    const vec2 step_size = radius / (float(StepsCount) * vec2(g_params.img_size));

    const float phi_rand = g_params.rand.x + Bayer4x4(gl_GlobalInvocationID.xy, 0);
    const float offset_rand = fract(g_params.rand.y + Bayer4x4(gl_GlobalInvocationID.xy, 1));

    float res = 0.0;
    for (int i = 0; i < DirectionsCount; ++i) {
        const float phi = 2.0 * M_PI * fract(float(i) / float(DirectionsCount) + phi_rand);
        vec2 curr_step = (1.0 / vec2(g_params.img_size)) + offset_rand * step_size;
        const vec3 dir = vec3(cos(phi), sin(phi), 0.0);

        vec2 horizons = vec2(-1.0);

        for (int j = 0; j < StepsCount; ++j) {
            const vec2 offset = dir.xy * curr_step;
            curr_step += step_size;
            { // positive direction
                const float depth_fetch = textureLod(g_depth_tex, pix_uv + offset, 0.0).x;
                const float depth_lin = LinearizeDepth(depth_fetch, g_params.clip_info);
                const vec3 dir_vs = ReconstructViewPosition(pix_uv + offset, g_params.frustum_info, -depth_lin, 0.0 /* is_ortho */) - center_point_vs;

                const float dist2 = dot(dir_vs, dir_vs);
                float cosh = inversesqrt(dist2) * dot(dir_vs, view_dir_vs) - Falloff(dist2);
                cosh = mix(cosh, horizons.x, 0.8 * float(j) / float(StepsCount));
                horizons.x = max(horizons.x, cosh);
            }
            { // negative direction
                const float depth_fetch = textureLod(g_depth_tex, pix_uv - offset, 0.0).x;
                const float depth_lin = LinearizeDepth(depth_fetch, g_params.clip_info);
                const vec3 dir_vs = ReconstructViewPosition(pix_uv - offset, g_params.frustum_info, -depth_lin, 0.0 /* is_ortho */) - center_point_vs;

                const float dist2 = dot(dir_vs, dir_vs);
                float cosh = inversesqrt(dist2) * dot(dir_vs, view_dir_vs) - Falloff(dist2);
                cosh = mix(cosh, horizons.y, 0.8 * float(j) / float(StepsCount));
                horizons.y = max(horizons.y, cosh);
            }
        }

        horizons = acos(horizons);

        // calculate gamma
		vec3 bitangent	= normalize(cross(dir, view_dir_vs));
		vec3 tangent	= cross(view_dir_vs, bitangent);
		vec3 nx			= center_normal_vs - bitangent * dot(center_normal_vs, bitangent);

		float nnx		= length(nx);
		float invnnx	= 1.0 / (nnx + 1e-6);			// to avoid division with zero
		float cosxi		= dot(nx, tangent) * invnnx;	// xi = gamma + PI_HALF
		float gamma		= acos(cosxi) - 0.5 * M_PI;
		float cosgamma	= dot(nx, view_dir_vs) * invnnx;
		float singamma2	= -2.0 * cosxi;					// cos(x + PI_HALF) = -sin(x)

		// clamp to normal hemisphere
		horizons.x = gamma + max(-horizons.x - gamma, -0.5 * M_PI);
		horizons.y = gamma + min(horizons.y - gamma, 0.5 * M_PI);

		// Riemann integral is additive
		res += /*nnx */ 0.25 * (
			(horizons.x * singamma2 + cosgamma - cos(2.0 * horizons.x - gamma)) +
			(horizons.y * singamma2 + cosgamma - cos(2.0 * horizons.y - gamma)));
    }

    // PDF = 1 / pi and must normalize with pi as of Lambert
	res = res / float(DirectionsCount);

    imageStore(g_out_img, ivec2(gl_GlobalInvocationID.xy), vec4(res));
}
