#version 430 core
#extension GL_EXT_control_flow_attributes : enable

#include "_fs_common.glsl"
#include "sun_shadows_interface.h"

#pragma multi_compile _ SS_SHADOW

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = DEPTH_LIN_TEX_SLOT) uniform sampler2D g_depth_lin_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = SHADOW_DEPTH_TEX_SLOT) uniform sampler2DShadow g_shadow_depth_tex;
layout(binding = SHADOW_DEPTH_TEX_VAL_SLOT) uniform sampler2D g_shadow_depth_val_tex;
layout(binding = SHADOW_COLOR_TEX_SLOT) uniform sampler2D g_shadow_color_tex;

layout(binding = OUT_SHADOW_IMG_SLOT, rgba8) uniform restrict writeonly image2D g_out_shadow_img;

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#define MAX_TRACE_DIST 0.15
#define MAX_TRACE_STEPS 16
#define Z_THICKNESS 0.025
#define STRIDE (3.0 / g_shrd_data.res_and_fres.x)

float LinearDepthFetch_Nearest(const vec2 hit_uv) {
    return LinearizeDepth(textureLod(g_depth_tex, hit_uv, 0.0).x, g_shrd_data.clip_info);
}

float LinearDepthFetch_Bilinear(const vec2 hit_uv) {
    return LinearizeDepth(textureLod(g_depth_lin_tex, hit_uv, 0.0).x, g_shrd_data.clip_info);
}

#include "ss_trace_simple.glsl.inl"

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
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

    const vec4 pos_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const float dot_N_L = saturate(dot(normal_ws, g_shrd_data.sun_dir.xyz));

    vec3 final_color = vec3(1.0);
    if (dot_N_L > -0.01) {
        float visibility = 1.0;

#ifdef SS_SHADOW
        if (lin_depth < 30.0) {
            vec2 hit_pixel;
            vec3 hit_point;

            float occlusion = 0.0;

            const float jitter = 0.0;//Bayer4x4(uvec2(icoord), 0);
            const vec3 pos_vs = (g_shrd_data.view_from_world * vec4(pos_ws, 1.0)).xyz;
            const vec3 sun_dir_vs = (g_shrd_data.view_from_world * vec4(g_shrd_data.sun_dir.xyz, 0.0)).xyz;
            if (IntersectRay(pos_vs, sun_dir_vs, jitter, hit_pixel, hit_point)) {
                occlusion = 1.0;
            }

            // view distance falloff
            occlusion *= saturate(30.0 - lin_depth);
            // shadow fadeout
            occlusion *= saturate(10.0 * (MAX_TRACE_DIST - distance(hit_point, pos_vs)));
            // fix shadow terminator
            occlusion *= saturate(abs(8.0 * dot_N_L));

            visibility -= occlusion;
        }
#endif

        const vec2 cascade_offsets[4] = vec2[4](
            vec2(0.0, 0.0),
            vec2(0.25, 0.0),
            vec2(0.0, 0.5),
            vec2(0.25, 0.5)
        );

        const vec2 shadow_offsets = get_shadow_offsets(dot_N_L);
        vec3 pos_ws_biased = pos_ws;
        pos_ws_biased += 0.001 * shadow_offsets.x * normal_ws;
        pos_ws_biased += 0.003 * shadow_offsets.y * g_shrd_data.sun_dir.xyz;

        vec4 g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2;
        [[unroll]] for (int i = 0; i < 4; i++) {
            vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * vec4(pos_ws_biased, 1.0)).xyz;
            shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
            shadow_uvs.xy *= vec2(0.25, 0.5);
            shadow_uvs.xy += cascade_offsets[i];
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

        final_color = visibility * GetSunVisibilityExt(lin_depth, g_shadow_depth_tex, g_shadow_depth_val_tex, g_shadow_color_tex,
                                                       transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)), g_params.softness_factor, hash);
    }

    imageStore(g_out_shadow_img, icoord, vec4(final_color, 1.0));
}
