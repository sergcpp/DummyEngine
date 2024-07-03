#version 430 core
#ifndef NO_SUBGROUP
#extension GL_KHR_shader_subgroup_ballot : enable
#endif

#include "_cs_common.glsl"
#include "_rt_common.glsl"
#include "_principled.glsl"
#include "gi_cache_common.glsl"
#include "ssr_common.glsl"
#include "ssr_trace_hq_interface.h"

#pragma multi_compile _ GI_CACHE
#pragma multi_compile _ NO_SUBGROUP

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = COLOR_TEX_SLOT) uniform sampler2D g_color_tex;
layout(binding = NORMAL_TEX_SLOT) uniform usampler2D g_normal_tex;
layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;

#ifdef GI_CACHE
    layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
    layout(binding = SPECULAR_TEX_SLOT) uniform usampler2D g_specular_tex;
    layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;

    layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
    layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
    layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;
#endif

layout(std430, binding = IN_RAY_LIST_SLOT) readonly buffer InRayList {
    uint g_in_ray_list[];
};

layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;
layout(std430, binding = OUT_RAY_LIST_SLOT) writeonly buffer OutRayList {
    uint g_out_ray_list[];
};
layout(std430, binding = INOUT_RAY_COUNTER_SLOT) coherent buffer RayCounter {
    uint g_inout_ray_counter[];
};

#include "ss_trace_common.glsl.inl"

void StoreRay(uint ray_index, uvec2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal) {
    g_out_ray_list[ray_index] = PackRay(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
}

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

vec3 ShadeHitPoint(const vec2 uv, const vec3 hit_point_vs, const vec3 refl_ray_vs, const float hit_t, const float first_roughness) {
    vec3 approx_spec = vec3(0.0);
#ifdef GI_CACHE
    const vec3 base_color = textureLod(g_albedo_tex, uv, 0.0).rgb;
    const vec4 normal = UnpackNormalAndRoughness(textureLod(g_normal_tex, uv, 0.0).r);
    const uint packed_mat_params = textureLod(g_specular_tex, uv, 0.0).r;

    vec4 mat_params0, mat_params1;
    UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);

    vec4 pos_ws = g_shrd_data.world_from_view * vec4(hit_point_vs, 1.0);
    pos_ws /= pos_ws.w;

    vec3 refl_ray_ws = (g_shrd_data.world_from_view * vec4(refl_ray_vs, 0.0)).xyz;

    const vec3 P = pos_ws.xyz;
    const vec3 I = normalize(g_shrd_data.cam_pos_and_exp.xyz - P);
    const vec3 N = normal.xyz;
    const float N_dot_V = saturate(dot(N, I));

    vec3 tint_color = vec3(0.0);

    const float base_color_lum = lum(base_color);
    if (base_color_lum > 0.0) {
        tint_color = base_color / base_color_lum;
    }

    const float roughness = normal.w;
    const float sheen = mat_params0.x;
    const float sheen_tint = mat_params0.y;
    const float specular = mat_params0.z;
    const float specular_tint = mat_params0.w;
    const float metallic = mat_params1.x;
    const float transmission = mat_params1.y;
    const float clearcoat = mat_params1.z;
    const float clearcoat_roughness = mat_params1.w;

    vec3 spec_tmp_col = mix(vec3(1.0), tint_color, specular_tint);
    spec_tmp_col = mix(specular * 0.08 * spec_tmp_col, base_color, metallic);

    const float spec_ior = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
    const float spec_F0 = fresnel_dielectric_cos(1.0, spec_ior);

    // Approximation of FH (using shading normal)
    const float FN = (fresnel_dielectric_cos(dot(I, N), spec_ior) - spec_F0) / (1.0 - spec_F0);

    const vec3 approx_spec_col = mix(spec_tmp_col, vec3(1.0), FN * (1.0 - roughness));
    const float spec_color_lum = lum(approx_spec_col);

    const lobe_weights_t lobe_weights = get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat);

    const vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

    const float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
    const float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
    const float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

    // Approximation of FH (using shading normal)
    const float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);
    const vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

    const ltc_params_t ltc = SampleLTC_Params(g_ltc_luts, N_dot_V, roughness, clearcoat_roughness2);

    for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
        const float weight = get_volume_blend_weight(P, g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
        if (weight > 0.0) {
            if (lobe_weights.specular > 0.0) {
                const vec3 refl_dir = reflect(refl_ray_ws, N);
                vec3 avg_radiance = get_volume_irradiance(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(refl_ray_ws, g_shrd_data.probe_volumes[i].spacing.xyz), refl_dir,
                                                            g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
                avg_radiance *= approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y;
                avg_radiance *= mix(1.0, saturate(hit_t / (0.5 * length(g_shrd_data.probe_volumes[i].spacing.xyz))), saturate(16.0 * first_roughness));
                approx_spec += (1.0 / M_PI) * avg_radiance;
            }
            break;
        }
    }
#endif
    return textureLod(g_color_tex, uv, 0.0).rgb + compress_hdr(approx_spec);
}

void main() {
    uint ray_index = gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex;
    if (ray_index >= g_inout_ray_counter[1]) return;
    uint packed_coords = g_in_ray_list[ray_index];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    ivec2 pix_uvs = ivec2(ray_coords);
    vec2 norm_uvs = (vec2(pix_uvs) + 0.5) / g_shrd_data.res_and_fres.xy;

    vec4 norm_rough = UnpackNormalAndRoughness(texelFetch(g_normal_tex, pix_uvs, 0).x);
    const float roughness = norm_rough.w * norm_rough.w;

    const float depth = texelFetch(g_depth_tex, pix_uvs, 0).r;

    const vec3 normal_ws = norm_rough.xyz;
    const vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

    const vec3 ray_origin_ss = vec3(norm_uvs, depth);
    const vec4 ray_origin_cs = vec4(2.0 * ray_origin_ss.xy - 1.0, ray_origin_ss.z, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);
    const float view_z = -ray_origin_vs.z;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec2 u = texelFetch(g_noise_tex, pix_uvs % 128, 0).rg;
    vec3 refl_ray_vs = SampleReflectionVector(view_ray_vs, normal_vs, roughness, u);

    vec3 hit_point_cs, hit_point_vs;
    vec3 out_color = vec3(0.0);
    bool hit_found = IntersectRay(ray_origin_ss, ray_origin_vs.xyz, refl_ray_vs, g_depth_tex, g_normal_tex, hit_point_cs, hit_point_vs);
    //hit_found = false;
    if (hit_found) {
        vec2 uv = hit_point_cs.xy;
#if defined(VULKAN)
        uv.y = -uv.y;
#endif // VULKAN
        uv.xy = 0.5 * uv.xy + 0.5;

        out_color += ShadeHitPoint(uv, hit_point_vs, refl_ray_vs, length(hit_point_vs - ray_origin_vs.xyz), roughness);
    }

    { // schedule rt rays
        bool needs_ray = !hit_found;
#ifndef NO_SUBGROUP
        uvec4 needs_ray_ballot = subgroupBallot(needs_ray);
        uint local_ray_index_in_wave = subgroupBallotExclusiveBitCount(needs_ray_ballot);
        uint wave_ray_count = subgroupBallotBitCount(needs_ray_ballot);

        uint base_ray_index = 0;
        if (subgroupElect()) {
            base_ray_index = atomicAdd(g_inout_ray_counter[4], wave_ray_count);
        }
        base_ray_index = subgroupBroadcastFirst(base_ray_index);
        if (needs_ray) {
            uint ray_index = base_ray_index + local_ray_index_in_wave;
            StoreRay(ray_index, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);
        }
#else
        if (needs_ray) {
            uint ray_index = atomicAdd(g_inout_ray_counter[4], 1);
            StoreRay(ray_index, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);
        }
#endif
    }

    float ray_len = hit_found ? distance(hit_point_vs, ray_origin_vs.xyz) : 0.0;
    ray_len = GetNormHitDist(ray_len, view_z, roughness);

    imageStore(g_out_color_img, pix_uvs, vec4(out_color, ray_len));

    const ivec2 copy_target = pix_uvs ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        const ivec2 copy_coords = ivec2(copy_target.x, pix_uvs.y);
        imageStore(g_out_color_img, copy_coords, vec4(out_color, ray_len));
    }
    if (copy_vertical) {
        const ivec2 copy_coords = ivec2(pix_uvs.x, copy_target.y);
        imageStore(g_out_color_img, copy_coords, vec4(out_color, ray_len));
    }
    if (copy_diagonal) {
        const ivec2 copy_coords = copy_target;
        imageStore(g_out_color_img, copy_coords, vec4(out_color, ray_len));
    }
}

