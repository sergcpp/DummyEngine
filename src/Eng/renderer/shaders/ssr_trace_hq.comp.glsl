#version 430 core
#extension GL_EXT_control_flow_attributes : enable
#ifndef NO_SUBGROUP
#extension GL_KHR_shader_subgroup_ballot : enable
#endif

#include "_cs_common.glsl"
#include "rt_common.glsl"
#include "principled_common.glsl"
#include "gi_cache_common.glsl"
#include "ssr_common.glsl"

#include "ssr_trace_hq_interface.h"

#pragma multi_compile _ LAYERED
#pragma multi_compile _ GI_CACHE
#pragma multi_compile _ NO_SUBGROUP

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = COLOR_TEX_SLOT) uniform sampler2D g_color_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_normal_tex;
#ifdef LAYERED
    layout(binding = OIT_DEPTH_BUF_SLOT) uniform usamplerBuffer g_oit_depth_buf;
#else
    layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;
#endif

#ifdef GI_CACHE
    layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
    layout(binding = SPEC_TEX_SLOT) uniform usampler2D g_specular_tex;
    layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;

    layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
    layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
    layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;
#endif

layout(std430, binding = IN_RAY_LIST_SLOT) readonly buffer InRayList {
    uint g_in_ray_list[];
};

#ifdef LAYERED
    layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform image2D g_out_color_img[OIT_REFLECTION_LAYERS];
#else
    layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;
#endif
layout(std430, binding = OUT_RAY_LIST_SLOT) restrict writeonly buffer OutRayList {
    uint g_out_ray_list[];
};
layout(std430, binding = INOUT_RAY_COUNTER_SLOT) restrict coherent buffer RayCounter {
    uint g_inout_ray_counter[];
};

#include "ss_trace_hierarchical.glsl.inl"

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

vec3 ShadeHitPoint(const vec2 uv, const vec4 norm_rough, const vec3 hit_point_vs, const vec3 refl_ray_vs, const float hit_t, out bool discard_intersection) {
    vec3 approx_spec = vec3(0.0);

    const float first_roughness = norm_rough.w * norm_rough.w;
    const vec3 refl_ray_ws = (g_shrd_data.world_from_view * vec4(refl_ray_vs, 0.0)).xyz;
    const vec3 hit_point_ws = (g_shrd_data.world_from_view * vec4(hit_point_vs, 1.0)).xyz;
#ifdef GI_CACHE
    const vec4 normal = UnpackNormalAndRoughness(textureLod(g_normal_tex, uv, 0.0).x);
    const vec3 base_color = textureLod(g_albedo_tex, uv, 0.0).xyz;
    const uint packed_mat_params = textureLod(g_specular_tex, uv, 0.0).x;

    vec4 mat_params0, mat_params1;
    UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);

    const vec3 P = hit_point_ws;
    const vec3 I = -refl_ray_ws;
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

    const lobe_masks_t lobe_masks = get_lobe_masks(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat);

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
            if ((lobe_masks.bits & LOBE_SPECULAR_BIT) != 0) {
                const vec3 refl_dir = reflect(refl_ray_ws, N);
                vec3 avg_radiance = get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(refl_ray_ws, g_shrd_data.probe_volumes[i].spacing.xyz), refl_dir,
                                                              g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz, false);
                avg_radiance *= approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y;
                avg_radiance *= mix(1.0, saturate(hit_t / (0.5 * length(g_shrd_data.probe_volumes[i].spacing.xyz))), saturate(16.0 * first_roughness));
                approx_spec += (1.0 / M_PI) * avg_radiance;
            }
            break;
        }
    }
#endif
    const vec4 ss_color = textureLod(g_color_tex, uv, 0.0);
    // Skip emissive surface
    discard_intersection = (ss_color.w > 0.0 && first_roughness > 1e-7);
    return ss_color.xyz + compress_hdr(approx_spec, g_shrd_data.cam_pos_and_exp.w);
}

void main() {
    const uint ray_index = gl_WorkGroupID.x * LOCAL_GROUP_SIZE_X + gl_LocalInvocationIndex;
    if (ray_index >= g_inout_ray_counter[1]) return;
#ifndef LAYERED
    const uint packed_coords = g_in_ray_list[ray_index];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    const ivec2 pix_uvs = ivec2(ray_coords);
    const vec2 norm_uvs = (vec2(pix_uvs) + 0.5) / g_shrd_data.res_and_fres.xy;

    vec4 norm_rough = UnpackNormalAndRoughness(texelFetch(g_normal_tex, pix_uvs, 0).x);
    const float roughness = norm_rough.w * norm_rough.w;

    const float depth = texelFetch(g_depth_tex, pix_uvs, 0).x;

    const vec3 normal_ws = norm_rough.xyz;
    const vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

    const vec3 ray_origin_ss = vec3(norm_uvs, depth);
    const vec4 ray_origin_cs = vec4(2.0 * ray_origin_ss.xy - 1.0, ray_origin_ss.z, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);
    const float view_z = -ray_origin_vs.z;

    const vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    const vec2 u = texelFetch(g_noise_tex, pix_uvs % 128, 0).xy;
    const vec3 refl_ray_vs = SampleReflectionVector(view_ray_vs, normal_vs, roughness, u);
#else
    const uint packed_coords = g_in_ray_list[2 * ray_index + 0];
    const uint packed_dir = g_in_ray_list[2 * ray_index + 1];

    uvec2 ray_coords;
    uint layer_index;
    UnpackRayCoords(packed_coords, ray_coords, layer_index);

    const vec2 oct_dir = vec2(packed_dir & 0xffffu, (packed_dir >> 16) & 0xffffu) / 65535.0;
    const vec3 refl_ray_ws = UnpackUnitVector(oct_dir);
    const vec3 refl_ray_vs = normalize((g_shrd_data.view_from_world * vec4(refl_ray_ws, 0.0)).xyz);

    int frag_index = int(layer_index) * g_shrd_data.ires_and_ifres.x * g_shrd_data.ires_and_ifres.y;
    frag_index += int(ray_coords.y) * g_shrd_data.ires_and_ifres.x + int(ray_coords.x);
    const float depth = uintBitsToFloat(texelFetch(g_oit_depth_buf, frag_index).x);
    const float roughness = 0.0;

    const ivec2 pix_uvs = ivec2(ray_coords);
    const vec2 norm_uvs = (vec2(ray_coords) + 0.5) / g_shrd_data.res_and_fres.xy;
    const vec3 ray_origin_ss = vec3(norm_uvs, depth);
    const vec4 ray_origin_cs = vec4(2.0 * ray_origin_ss.xy - 1.0, ray_origin_ss.z, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);
    const float view_z = -ray_origin_vs.z;
    const vec3 view_ray_vs = normalize(ray_origin_vs);
    vec4 norm_rough = vec4(0.0); // stub
#endif

    vec3 hit_point_cs, hit_point_vs, hit_normal_vs;
    bool hit_found = IntersectRay(ray_origin_ss, ray_origin_vs, refl_ray_vs, g_depth_tex, g_normal_tex, hit_point_cs, hit_point_vs, hit_normal_vs);

    vec3 out_color = vec3(0.0);
    if (hit_found) {
        vec2 uv = hit_point_cs.xy;
#if defined(VULKAN)
        uv.y = -uv.y;
#endif // VULKAN
        uv.xy = 0.5 * uv.xy + 0.5;

        bool discard_intersection;
        out_color = ShadeHitPoint(uv, norm_rough, hit_point_vs, refl_ray_vs, length(hit_point_vs - ray_origin_vs.xyz), discard_intersection);
        hit_found = !discard_intersection;
    }


    { // schedule rt rays
        bool needs_ray = !hit_found;
#ifndef NO_SUBGROUP
        uvec4 needs_ray_ballot = subgroupBallot(needs_ray);
        uint local_ray_index_in_wave = subgroupBallotExclusiveBitCount(needs_ray_ballot);
        uint wave_ray_count = subgroupBallotBitCount(needs_ray_ballot);

        uint base_ray_index = 0;
        if (subgroupElect()) {
            base_ray_index = atomicAdd(g_inout_ray_counter[6], wave_ray_count);
        }
        base_ray_index = subgroupBroadcastFirst(base_ray_index);
        if (needs_ray) {
            uint ray_index = base_ray_index + local_ray_index_in_wave;
#ifndef LAYERED
            g_out_ray_list[ray_index] = packed_coords;
#else // LAYERED
            g_out_ray_list[2 * ray_index + 0] = packed_coords;
            g_out_ray_list[2 * ray_index + 1] = packed_dir;
#endif // LAYERED
        }
#else // NO_SUBGROUP
        if (needs_ray) {
            uint ray_index = atomicAdd(g_inout_ray_counter[6], 1);
#ifndef LAYERED
            g_out_ray_list[ray_index] = packed_coords;
#else // LAYERED
            g_out_ray_list[2 * ray_index + 0] = packed_coords;
            g_out_ray_list[2 * ray_index + 1] = packed_dir;
#endif // LAYERED
        }
#endif // NO_SUBGROUP
    }

    float ray_len = hit_found ? distance(hit_point_vs, ray_origin_vs.xyz) : 0.0;
    ray_len = GetNormHitDist(ray_len, view_z, roughness);

#ifndef LAYERED
    const vec3 prev_color = imageLoad(g_out_color_img, pix_uvs).xyz;
    imageStore(g_out_color_img, pix_uvs, vec4(prev_color + out_color, ray_len));

    const ivec2 copy_target = pix_uvs ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        const ivec2 copy_coords = ivec2(copy_target.x, pix_uvs.y);
        const vec3 prev_color = imageLoad(g_out_color_img, copy_coords).xyz;
        imageStore(g_out_color_img, copy_coords, vec4(prev_color + out_color, ray_len));
    }
    if (copy_vertical) {
        const ivec2 copy_coords = ivec2(pix_uvs.x, copy_target.y);
        const vec3 prev_color = imageLoad(g_out_color_img, copy_coords).xyz;
        imageStore(g_out_color_img, copy_coords, vec4(prev_color + out_color, ray_len));
    }
    if (copy_diagonal) {
        const ivec2 copy_coords = copy_target;
        const vec3 prev_color = imageLoad(g_out_color_img, copy_coords).xyz;
        imageStore(g_out_color_img, copy_coords, vec4(prev_color + out_color, ray_len));
    }
#else
    [[dont_flatten]] if (layer_index == 0) {
        imageStore(g_out_color_img[0], pix_uvs / 2, vec4(out_color, 1.0));
    } else if (layer_index == 1) {
        imageStore(g_out_color_img[1], pix_uvs / 2, vec4(out_color, 1.0));
    }
#endif
}
