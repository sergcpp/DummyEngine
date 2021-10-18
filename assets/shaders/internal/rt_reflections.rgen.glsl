#version 460
#extension GL_EXT_ray_tracing : require

#include "_rt_common.glsl"
#include "ssr_common.glsl"
#include "rt_reflections_interface.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = SPEC_TEX_SLOT) uniform sampler2D s_spec_texture;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D s_depth_texture;
layout(binding = NORM_TEX_SLOT) uniform sampler2D s_norm_texture;
layout(binding = ROUGH_TEX_SLOT) uniform sampler2D rough_texture;

layout(std430, binding = RAY_LIST_SLOT) readonly buffer RayList {
    uint g_ray_list[];
};

layout(binding = SOBOL_BUF_SLOT) uniform highp usamplerBuffer sobol_seq_tex;
layout(binding = SCRAMLING_TILE_BUF_SLOT) uniform highp usamplerBuffer scrambling_tile_tex;
layout(binding = RANKING_TILE_BUF_SLOT) uniform highp usamplerBuffer ranking_tile_tex;

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT tlas;
layout(binding = OUT_COLOR_IMG_SLOT, r11f_g11f_b10f) uniform image2D out_color_img;
layout(binding = OUT_RAYLEN_IMG_SLOT, r16f) uniform image2D out_raylen_img;

layout(location = 0) rayPayloadEXT RayPayload pld;


//
// https://eheitzresearch.wordpress.com/762-2/
//
float SampleRandomNumber(in uvec2 pixel, in uint sample_index, in uint sample_dimension)
{
    // wrap arguments
    uint pixel_i = pixel.x & 127u;
    uint pixel_j = pixel.y & 127u;
    sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

    // xor index based on optimized ranking
    uint ranked_sample_index = sample_index ^ texelFetch(ranking_tile_tex, int((sample_dimension & 7u) + (pixel_i + pixel_j * 128u) * 8u)).r;

    // fetch value in sequence
    uint value = texelFetch(sobol_seq_tex, int(sample_dimension + ranked_sample_index * 256u)).r;

    // if the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ texelFetch(scrambling_tile_tex, int((sample_dimension & 7u) + (pixel_i + pixel_j * 128u) * 8u)).r;

    // convert to float and return
    return (float(value) + 0.5) / 256.0;
}

vec2 SampleRandomVector2D(uvec2 pixel) {
    uint frame_index = floatBitsToUint(shrd_data.uTaaInfo[2]);
    vec2 u = vec2(mod(SampleRandomNumber(pixel, 0, 0u) + float(frame_index & 0xFFu) * GOLDEN_RATIO, 1.0),
                  mod(SampleRandomNumber(pixel, 0, 1u) + float(frame_index & 0xFFu) * GOLDEN_RATIO, 1.0));
    return u;
}

vec3 SampleReflectionVector(vec3 view_direction, vec3 normal, float roughness, ivec2 dispatch_thread_id) {
    mat3 tbn_transform = CreateTBN(normal);
    vec3 view_direction_tbn = tbn_transform * (-view_direction);

    vec2 u = SampleRandomVector2D(dispatch_thread_id);
    
    vec3 sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);
#ifdef PERFECT_REFLECTIONS
        sampled_normal_tbn = vec3(0.0, 0.0, 1.0); // Overwrite normal sample to produce perfect reflection.
#endif
    
    vec3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

    // Transform reflected_direction back to the initial space.
    mat3 inv_tbn_transform = transpose(tbn_transform);
    return (inv_tbn_transform * reflected_direction_tbn);
}

void main() {
    uint packed_coords = g_ray_list[gl_LaunchIDEXT.x];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    ivec2 icoord = ivec2(ray_coords);

    vec4 specular = texelFetch(s_spec_texture, icoord, 0);
    if ((specular.r + specular.g + specular.b) < 0.0001) return;

    float depth = texelFetch(s_depth_texture, icoord, 0).r;
    vec3 normal_ws = 2.0 * texelFetch(s_norm_texture, icoord, 0).xyz - 1.0;
    vec3 normal_vs = normalize((shrd_data.uViewMatrix * vec4(normal_ws, 0.0)).xyz);
    
    float roughness = texelFetch(rough_texture, icoord, 0).r;
    
    const vec2 px_center = vec2(icoord) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(params.img_size);
    
#if defined(VULKAN)
    vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);    
#endif // VULKAN

    vec4 ray_origin_vs = shrd_data.uInvProjMatrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec3 refl_ray_vs = SampleReflectionVector(view_ray_vs, normal_vs, roughness, icoord);
    vec3 refl_ray_ws = (shrd_data.uInvViewMatrix * vec4(refl_ray_vs.xyz, 0.0)).xyz;

    vec4 ray_origin_ws = shrd_data.uInvViewMatrix * ray_origin_vs;
    ray_origin_ws /= ray_origin_ws.w;

    pld.cone_width = params.pixel_spread_angle * (-ray_origin_vs.z);

    { // trace through bvh tree
        const uint ray_flags = gl_RayFlagsCullBackFacingTrianglesEXT;
        const float t_min = 0.001;
        const float t_max = 1000.0;
        
        traceRayEXT(tlas,           // topLevel
                ray_flags,          // rayFlags
                0xff,               // cullMask
                0,                  // sbtRecordOffset
                0,                  // sbtRecordStride
                0,                  // missIndex
                ray_origin_ws.xyz,  // origin
                t_min,              // Tmin
                refl_ray_ws.xyz,    // direction
                t_max,              // Tmax
                0                   // payload
                );
    }

    imageStore(out_color_img, icoord, vec4(pld.col.rgb, 1.0));
    imageStore(out_raylen_img, icoord, vec4(pld.cone_width));
}
