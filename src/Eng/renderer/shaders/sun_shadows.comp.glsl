#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

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

#define ROTATE_BLOCKER_SEARCH 1
#define GATHER4_BLOCKER_SEARCH 1

vec2 BlockerSearch(sampler2D shadow_val_tex, vec3 shadow_uv, float search_radius, vec4 rotator) {
    const vec2 shadow_size = vec2(float(SHADOWMAP_RES), float(SHADOWMAP_RES) / 2.0);

    float avg_distance = 0.0;
    float count = 0.0;

#if !ROTATE_BLOCKER_SEARCH
    rotator = vec4(1, 0, 0, 1);
#endif

    for (int i = 0; i < 8; ++i) {
        vec2 uv = shadow_uv.xy + search_radius * RotateVector(rotator, g_poisson_disk_8[i] / shadow_size);
#if GATHER4_BLOCKER_SEARCH
        vec4 depth = textureGather(g_shadow_val_tex, uv, 0);
        vec4 valid = vec4(lessThan(depth, vec4(shadow_uv.z)));
        avg_distance += dot(valid, depth);
        count += dot(valid, vec4(1.0));
#else
        float depth = textureLod(g_shadow_val_tex, uv, 0.0).x;
        if (depth < shadow_uv.z) {
            avg_distance += depth;
            count += 1.0;
        }
#endif
    }

    return vec2(avg_distance, count);
}

float GetCascadeVisibility(int cascade, sampler2DShadow shadow_tex, sampler2D shadow_val_tex, mat4x3 aVertexShUVs, vec4 rotator) {
    const vec2 ShadowSizePx = vec2(float(SHADOWMAP_RES), float(SHADOWMAP_RES) / 2.0);
    const float MinShadowRadiusPx = 1.5; // needed to hide blockyness
    const float MaxShadowRadiusPx = 16.0;

    float visibility = 0.0;

    vec2 blocker = BlockerSearch(shadow_val_tex, aVertexShUVs[cascade], MaxShadowRadiusPx, rotator);
    if (blocker.y < 0.5) {
        return 1.0;
    }
    blocker.x /= blocker.y;

    const float filter_radius_px = clamp(g_params.softness_factor[cascade] * abs(blocker.x - aVertexShUVs[cascade].z), MinShadowRadiusPx, MaxShadowRadiusPx);
    for (int i = 0; i < 16; ++i) {
        vec2 uv = aVertexShUVs[cascade].xy + filter_radius_px * RotateVector(rotator, g_poisson_disk_16[i] / ShadowSizePx);
        visibility += textureLod(g_shadow_tex, vec3(uv, aVertexShUVs[cascade].z), 0.0);
    }
    visibility /= 16.0;

    return visibility;
}

float GetSunVisibility(float frag_depth, sampler2DShadow shadow_tex, sampler2D shadow_val_tex, mat4x3 aVertexShUVs, float hash) {
    const float angle = hash * 2.0 * M_PI;
    const float ca = cos(angle), sa = sin(angle);
    const vec4 rotator = vec4(ca, sa, -sa, ca);

    if (frag_depth < SHADOWMAP_CASCADE0_DIST) {
        return GetCascadeVisibility(0, shadow_tex, shadow_val_tex, aVertexShUVs, rotator);
    } else if (frag_depth < SHADOWMAP_CASCADE1_DIST) {
        return GetCascadeVisibility(1, shadow_tex, shadow_val_tex, aVertexShUVs, rotator);
    } else if (frag_depth < SHADOWMAP_CASCADE2_DIST) {
        return GetCascadeVisibility(2, shadow_tex, shadow_val_tex, aVertexShUVs, rotator);
    }
    return GetCascadeVisibility(3, shadow_tex, shadow_val_tex, aVertexShUVs, rotator);
}

float HashWorldPosition(vec2 in_uv, vec3 pos_ws, vec3 pos_ws_biased) {
    vec3 pos_ws_10, pos_ws_01;

    { // sample position offseted by X
        vec2 in_uv10 = in_uv + vec2(1.0, 0.0) / vec2(g_params.img_size);
        float depth10 = textureLod(g_depth_tex, in_uv10, 0.0).r;
#if defined(VULKAN)
        vec4 ray_origin_cs10 = vec4(2.0 * in_uv10 - 1.0, depth10, 1.0);
        ray_origin_cs10.y = -ray_origin_cs10.y;
#else // VULKAN
        vec4 ray_origin_cs10 = vec4(2.0 * vec3(in_uv10, depth10) - 1.0, 1.0);
#endif // VULKAN
        vec4 pos_ws10 = g_shrd_data.world_from_clip * ray_origin_cs10;
        pos_ws_10 = pos_ws10.xyz / pos_ws10.w;
    }
    { // sample position offseted by Y
        vec2 in_uv01 = in_uv + vec2(0.0, 1.0) / vec2(g_params.img_size);
        float depth01 = textureLod(g_depth_tex, in_uv01, 0.0).r;
#if defined(VULKAN)
        vec4 ray_origin_cs01 = vec4(2.0 * in_uv01 - 1.0, depth01, 1.0);
        ray_origin_cs01.y = -ray_origin_cs01.y;
#else // VULKAN
        vec4 ray_origin_cs01 = vec4(2.0 * vec3(in_uv01, depth01) - 1.0, 1.0);
#endif // VULKAN
        vec4 pos_ws01 = g_shrd_data.world_from_clip * ray_origin_cs01;
        pos_ws_01 = pos_ws01.xyz / pos_ws01.w;
    }

    vec3 dx = pos_ws_10 - pos_ws, dy = pos_ws_01 - pos_ws;
    float max_deriv = max(length(dx), length(dy));

    const float HashScale = 0.25;
    float pix_scale = 1.0 / (HashScale * max_deriv);
    pix_scale = exp2(ceil(log2(pix_scale)));

    return hash3D(floor(pix_scale * pos_ws_biased));
}

void main() {
    ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    if (g_params.enabled.x < 0.5) {
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

        float hash = HashWorldPosition(in_uv, pos_ws.xyz, pos_ws_biased);

        //visibility = GetSunVisibility(lin_depth, g_shadow_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)));
        visibility = GetSunVisibility(lin_depth, g_shadow_tex, g_shadow_val_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)), hash);
    }

    imageStore(g_out_shadow_img, icoord, vec4(visibility));
}
