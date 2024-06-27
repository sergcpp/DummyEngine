#version 430 core

#include "_cs_common.glsl"
#include "gi_cache_common.glsl"
#include "probe_sample_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORMAL_TEX_SLOT) uniform usampler2D g_normal_tex;
layout(binding = SSAO_TEX_SLOT) uniform sampler2D g_ssao_tex;

layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    const vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    const float depth = texelFetch(g_depth_tex, icoord, 0).r;

    vec4 pos_cs = vec4(norm_uvs, depth, 1.0);
#if defined(VULKAN)
    pos_cs.xy = 2.0 * pos_cs.xy - 1.0;
    pos_cs.y = -pos_cs.y;
#else // VULKAN
    pos_cs.xyz = 2.0 * pos_cs.xyz - 1.0;
#endif // VULKAN

    vec4 pos_ws = g_shrd_data.world_from_clip * pos_cs;
    pos_ws /= pos_ws.w;

    const vec3 P = pos_ws.xyz;
    const vec3 I = normalize(P - g_shrd_data.cam_pos_and_exp.xyz);

    const vec4 normal = UnpackNormalAndRoughness(texelFetch(g_normal_tex, icoord, 0).r);

    vec3 final_color = vec3(0.0);
    for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
        const float weight = get_volume_blend_weight(P, g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
        if (weight > 0.0) {
            final_color += weight * get_volume_irradiance(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(normal.xyz, I, g_shrd_data.probe_volumes[i].spacing.xyz), normal.xyz,
                                                          g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
            if (weight < 1.0 && i < PROBE_VOLUMES_COUNT - 1) {
                final_color += (1.0 - weight) * get_volume_irradiance(i + 1, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(normal.xyz, I, g_shrd_data.probe_volumes[i + 1].spacing.xyz), normal.xyz,
                                                                      g_shrd_data.probe_volumes[i + 1].scroll.xyz, g_shrd_data.probe_volumes[i + 1].origin.xyz, g_shrd_data.probe_volumes[i + 1].spacing.xyz);
            }
            break;
        }
    }
    final_color *= texelFetch(g_ssao_tex, icoord, 0).x;

    imageStore(g_out_color_img, icoord, vec4(compress_hdr(final_color / M_PI), 1.0));
}