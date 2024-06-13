#version 430 core

#include "_fs_common.glsl"
#include "sun_shadows_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = SHADOW_TEX_VAL_SLOT) uniform sampler2D g_shadow_val_tex;

layout(binding = OUT_SHADOW_IMG_SLOT, r8) uniform writeonly restrict image2D g_out_shadow_img;

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main() {
    ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    if (g_params.enabled < 0.5) {
        imageStore(g_out_shadow_img, icoord, vec4(1.0));
        return;
    }

    float depth = texelFetch(g_depth_tex, icoord, 0).r;
    float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);
    vec3 normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0).x).xyz;
    vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

    vec2 px_center = vec2(icoord) + 0.5;
    vec2 in_uv = px_center / vec2(g_params.img_size);

#if defined(VULKAN)
    vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);
#endif // VULKAN

    vec4 pos_ws = g_shrd_data.world_from_clip * ray_origin_cs;
    pos_ws /= pos_ws.w;

    const float dot_N_L = saturate(dot(normal_ws, g_shrd_data.sun_dir.xyz));

    float visibility = 0.0;
    if (dot_N_L > -0.01) {
        vec4 g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2;

        const vec2 offsets[4] = vec2[4](
            vec2(0.0, 0.0),
            vec2(0.25, 0.0),
            vec2(0.0, 0.5),
            vec2(0.25, 0.5)
        );

        const vec2 shadow_offsets = get_shadow_offsets(dot_N_L);
        vec3 pos_ws_biased = pos_ws.xyz;
        pos_ws_biased += 0.001 * shadow_offsets.x * normal_ws;
        pos_ws_biased += 0.003 * shadow_offsets.y * g_shrd_data.sun_dir.xyz;

        /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
            vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * vec4(pos_ws_biased, 1.0)).xyz;
    #if defined(VULKAN)
            shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
    #else // VULKAN
            shadow_uvs = 0.5 * shadow_uvs + 0.5;
    #endif // VULKAN
            shadow_uvs.xy *= vec2(0.25, 0.5);
            shadow_uvs.xy += offsets[i];
    #if defined(VULKAN)
            shadow_uvs.y = 1.0 - shadow_uvs.y;
    #endif // VULKAN

            g_vtx_sh_uvs0[i] = shadow_uvs[0];
            g_vtx_sh_uvs1[i] = shadow_uvs[1];
            g_vtx_sh_uvs2[i] = shadow_uvs[2];
        }

        float pix_scale = 3.0 / (g_params.pixel_spread_angle * lin_depth);
        pix_scale = exp2(ceil(log2(pix_scale)));
        const float hash = hash3D(floor(pix_scale * pos_ws_biased));

        //visibility = GetSunVisibility(lin_depth, g_shadow_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)));
        visibility = GetSunVisibility(lin_depth, g_shadow_tex, g_shadow_val_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)),
                                      g_params.softness_factor, hash);
    }

    imageStore(g_out_shadow_img, icoord, vec4(visibility));
}
