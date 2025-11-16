#version 430 core
#extension GL_EXT_control_flow_attributes : require
#if !defined(NO_SUBGROUP)
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require
#endif

#if defined(NO_SUBGROUP)
    #define subgroupMin(x) (x)
#endif

#if defined(SHADOW_JITTER)
    #define SIMPLIFIED_LTC_DIFFUSE 0
#endif

#define LTC_SHARED_MEM 64

#include "_fs_common.glsl"
#include "principled_common.glsl"
#include "taa_common.glsl"
#include "gbuffer_shade_interface.h"

#pragma multi_compile _ SHADOW_JITTER
#pragma multi_compile _ SS_SHADOW_ONE SS_SHADOW_MANY
#pragma multi_compile _ NO_SUBGROUP

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = DEPTH_LIN_TEX_SLOT) uniform sampler2D g_depth_lin_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_normal_tex;
layout(binding = SPEC_TEX_SLOT) uniform usampler2D g_specular_tex;

layout(binding = SHADOW_DEPTH_TEX_SLOT) uniform sampler2DShadow g_shadow_depth_tex;
layout(binding = SHADOW_DEPTH_VAL_TEX_SLOT) uniform sampler2D g_shadow_depth_val_tex;
layout(binding = SHADOW_COLOR_TEX_SLOT) uniform sampler2D g_shadow_color_tex;
layout(binding = SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = LIGHT_BUF_SLOT) uniform usamplerBuffer g_lights_buf;
layout(binding = DECAL_BUF_SLOT) uniform samplerBuffer g_decals_buf;
layout(binding = CELLS_BUF_SLOT) uniform usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform usamplerBuffer g_items_buf;
layout(binding = GI_TEX_SLOT) uniform sampler2D g_gi_tex;
layout(binding = SUN_SHADOW_TEX_SLOT) uniform sampler2D g_sun_shadow_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;
layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(binding = OUT_COLOR_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

#define MAX_TRACE_DIST 0.15
#define MAX_TRACE_STEPS 16
#define Z_THICKNESS 0.025
#define STRIDE (3.0 * g_shrd_data.ren_res.z)

float LinearDepthFetch_Nearest(const vec2 hit_uv) {
    return LinearizeDepth(textureLod(g_depth_tex, hit_uv, 0.0).x, g_shrd_data.clip_info);
}

float LinearDepthFetch_Bilinear(const vec2 hit_uv) {
    return LinearizeDepth(textureLod(g_depth_lin_tex, hit_uv, 0.0).x, g_shrd_data.clip_info);
}

#include "ss_trace_simple.glsl.inl"

vec3 LightVisibility(const _light_item_t litem, vec3 P, vec3 pos_vs, vec3 N, float lin_depth, vec4 rotator, const float hash) {
    int shadowreg_index = floatBitsToInt(litem.u_and_reg.w);
    [[dont_flatten]] if (shadowreg_index == -1) {
        return vec3(1.0);
    }

    vec3 from_light = normalize(P + 0.05 * (hash - 0.5) * N - litem.shadow_pos_and_tri_index.xyz);
    shadowreg_index += cubemap_face(from_light, litem.dir_and_spot.xyz, normalize(litem.u_and_reg.xyz), normalize(litem.v_and_blend.xyz));
    vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

    vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(P, 1.0);
    pp /= pp.w;

    pp.xy = pp.xy * 0.5 + 0.5;
    pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
#if defined(VULKAN)
    pp.y = 1.0 - pp.y;
#endif // VULKAN

    const vec2 ShadowSizePx = vec2(float(SHADOWMAP_RES), float(SHADOWMAP_RES / 2));
    const float MinShadowRadiusPx = 1.5; // needed to hide blockyness
#ifdef SHADOW_JITTER
    const float MaxShadowRadiusPx = 2.0;
#else // SHADOW_JITTER
    const float MaxShadowRadiusPx = 3.5;
#endif // SHADOW_JITTER

#if BLOCKER_SEARCH_USE_COLOR_ALPHA
    vec2 blocker = BlockerSearch(g_shadow_depth_val_tex, g_shadow_color_tex, pp.xyz, MaxShadowRadiusPx * (1.0 - pp.z), rotator);
#else
    vec2 blocker = BlockerSearch(g_shadow_depth_val_tex, pp.xyz, MaxShadowRadiusPx * (1.0 - pp.z), rotator);
#endif

    vec3 final_color = textureLod(g_shadow_color_tex, pp.xy, 0.0).xyz;
    if (blocker.y > 0.5) {
        blocker.x /= blocker.y;

        const float penumbra_size_approx = 2.0 * (ShadowSizePx.x * reg_tr.z) * abs(blocker.x - pp.z) * litem.pos_and_radius.w / (1.0 - blocker.x);
        const float filter_radius_px = clamp(penumbra_size_approx, MinShadowRadiusPx, MaxShadowRadiusPx);

        vec3 shadow_color = vec3(0.0);
        for (int i = 0; i < 16; ++i) {
            const vec2 uv = pp.xy + filter_radius_px * RotateVector(rotator, g_poisson_disk_16[i] / ShadowSizePx);
            const float shadow_visibility = textureLod(g_shadow_depth_tex, vec3(uv, pp.z), 0.0);
            shadow_color += shadow_visibility * textureLod(g_shadow_color_tex, uv, 0.0).xyz;
        }
        final_color = shadow_color / 16.0;
    }

#ifdef SS_SHADOW_MANY
    if (hsum(final_color) > 0.0 && lin_depth < 30.0) {
        vec2 hit_uv;
        vec3 hit_point;

        float occlusion = 0.0;
        const vec3 light_dir_vs = (g_shrd_data.view_from_world * vec4(normalize(litem.shadow_pos_and_tri_index.xyz - P), 0.0)).xyz;
        if (IntersectRay(pos_vs, light_dir_vs, hash, hit_uv, hit_point)) {
            const float shadow_vis = textureLod(g_albedo_tex, hit_uv, 0.0).w;
            if (shadow_vis > 0.5) {
                occlusion = 1.0;
            }
        }

        // view distance falloff
        occlusion *= saturate(30.0 - lin_depth);
        // shadow fadeout
        occlusion *= saturate(10.0 * (MAX_TRACE_DIST - distance(hit_point, pos_vs)));
        // fix shadow terminator
        occlusion *= saturate(abs(8.0 * dot(N, normalize(litem.shadow_pos_and_tri_index.xyz - P))));

        final_color *= (1.0 - occlusion);
    }
#endif // SS_SHADOW_MANY

    return final_color;
}

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    const vec2 norm_uvs = (vec2(icoord) + 0.5) * g_shrd_data.ren_res.zw;

    const float depth = texelFetch(g_depth_tex, icoord, 0).x;
    if (depth == 0.0) {
        return;
    }
    const float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);

    const float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    const int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

    const int cell_index = GetCellIndex(icoord.x, icoord.y, slice, g_shrd_data.ren_res.xy);

    const vec4 pos_cs = vec4(2.0 * norm_uvs - 1.0, depth, 1.0);
    const vec3 pos_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, pos_cs);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec3 base_color = texelFetch(g_albedo_tex, icoord, 0).xyz;
    const vec4 normal = UnpackNormalAndRoughness(texelFetch(g_normal_tex, icoord, 0).x);
    const uint packed_mat_params = texelFetch(g_specular_tex, icoord, 0).x;

    //
    // Artificial lights
    //

    const vec3 P = pos_ws.xyz;
    const vec3 I = normalize(g_shrd_data.cam_pos_and_exp.xyz - P);
    const vec3 N = normal.xyz;
    const float N_dot_V = saturate(dot(N, I));

    vec3 tint_color = vec3(0.0);

    const float base_color_lum = lum(base_color);
    if (base_color_lum > 0.0) {
        tint_color = base_color / base_color_lum;
    }

    vec4 mat_params0, mat_params1;
    UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);

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

    g_ltc[gl_LocalInvocationIndex] = SampleLTC_Params(g_ltc_luts, N_dot_V, roughness, clearcoat_roughness2);

    //
    // Evaluate artifitial lights
    //
    float pix_scale = 3.0 / (g_params.pixel_spread_angle * lin_depth);
    pix_scale = exp2(ceil(log2(pix_scale)));
    const float hash = hash3D(floor(pix_scale * P));
    const float angle = hash * 2.0 * M_PI;
    const float ca = cos(angle), sa = sin(angle);
    const vec4 rotator = vec4(ca, sa, -sa, ca);

    const float portals_specular_ltc_weight = smoothstep(0.0, 0.25, roughness);

    vec3 artificial_light = vec3(0.0);
    vec3 brightest_light_contribution = vec3(0.0), brightest_light_pos;
#if !defined(NO_SUBGROUP)
    [[dont_flatten]] if (subgroupAllEqual(cell_index)) {
        const int s_first_cell_index = subgroupBroadcastFirst(cell_index);
        const uvec2 s_cell_data = texelFetch(g_cells_buf, s_first_cell_index).xy;
        const uvec2 s_offset_and_lcount = uvec2(bitfieldExtract(s_cell_data.x, 0, 24), bitfieldExtract(s_cell_data.x, 24, 8));
        for (uint i = s_offset_and_lcount.x; i < s_offset_and_lcount.x + s_offset_and_lcount.y; ++i) {
            const uint s_item_data = texelFetch(g_items_buf, int(i)).x;
            const int s_li = int(bitfieldExtract(s_item_data, 0, 12));

            const _light_item_t litem = FetchLightItem(g_lights_buf, s_li);
            const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
            const bool is_diffuse = (floatBitsToUint(litem.col_and_type.w) & LIGHT_DIFFUSE_BIT) != 0;
            const bool is_specular = (floatBitsToUint(litem.col_and_type.w) & LIGHT_SPECULAR_BIT) != 0;

            lobe_masks_t _lobe_masks = lobe_masks;
            [[flatten]] if (is_portal) _lobe_masks.specular_mul *= portals_specular_ltc_weight;
            [[flatten]] if (!is_diffuse) _lobe_masks.bits &= ~LOBE_DIFFUSE_BIT;
            [[flatten]] if (!is_specular) _lobe_masks.bits &= ~(LOBE_SPECULAR_BIT | LOBE_CLEARCOAT_BIT);
            vec3 light_contribution = EvaluateLightSource_LTC(litem, P, I, N, _lobe_masks, g_ltc_luts,
                                                              sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
            if (all(equal(light_contribution, vec3(0.0)))) {
                continue;
            }
            if (is_portal) {
                // Sample environment to create slight color variation
                const vec3 rotated_dir = rotate_xz(normalize(litem.shadow_pos_and_tri_index.xyz - P), g_shrd_data.env_col.w);
                light_contribution *= textureLod(g_env_tex, rotated_dir, g_shrd_data.ambient_hack.w - 2.0).xyz;
            }
            light_contribution *= LightVisibility(litem, P, pos_vs, N, lin_depth, rotator, hash);
            if (lum(light_contribution) > lum(brightest_light_contribution)) {
                brightest_light_contribution = light_contribution;
                brightest_light_pos = litem.shadow_pos_and_tri_index.xyz;
            }
            artificial_light += light_contribution;
        }
    } else
#endif // !defined(NO_SUBGROUP)
    {
        const uvec2 v_cell_data = texelFetch(g_cells_buf, cell_index).xy;
        const uvec2 v_offset_and_lcount = uvec2(bitfieldExtract(v_cell_data.x, 0, 24), bitfieldExtract(v_cell_data.x, 24, 8));
        for (uint i = v_offset_and_lcount.x; i < v_offset_and_lcount.x + v_offset_and_lcount.y; ) {
            const uint v_item_data = texelFetch(g_items_buf, int(i)).x;
            const int v_li = int(bitfieldExtract(v_item_data, 0, 12));
            const int s_li = subgroupMin(v_li);
            [[flatten]] if (s_li == v_li) {
                ++i;
                const _light_item_t litem = FetchLightItem(g_lights_buf, s_li);
                const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
                const bool is_diffuse = (floatBitsToUint(litem.col_and_type.w) & LIGHT_DIFFUSE_BIT) != 0;
                const bool is_specular = (floatBitsToUint(litem.col_and_type.w) & LIGHT_SPECULAR_BIT) != 0;

                lobe_masks_t _lobe_masks = lobe_masks;
                [[flatten]] if (is_portal) _lobe_masks.specular_mul *= portals_specular_ltc_weight;
                [[flatten]] if (!is_diffuse) _lobe_masks.bits &= ~LOBE_DIFFUSE_BIT;
                [[flatten]] if (!is_specular) _lobe_masks.bits &= ~(LOBE_SPECULAR_BIT | LOBE_CLEARCOAT_BIT);
                vec3 light_contribution = EvaluateLightSource_LTC(litem, P, I, N, _lobe_masks, g_ltc_luts,
                                                                  sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
                if (all(equal(light_contribution, vec3(0.0)))) {
                    continue;
                }
                if (is_portal) {
                    // Sample environment to create slight color variation
                    const vec3 rotated_dir = rotate_xz(normalize(litem.shadow_pos_and_tri_index.xyz - P), g_shrd_data.env_col.w);
                    light_contribution *= textureLod(g_env_tex, rotated_dir, g_shrd_data.ambient_hack.w - 2.0).xyz;
                }
                light_contribution *= LightVisibility(litem, P, pos_vs, N, lin_depth, rotator, hash);
                if (lum(light_contribution) > lum(brightest_light_contribution)) {
                    brightest_light_contribution = light_contribution;
                    brightest_light_pos = litem.shadow_pos_and_tri_index.xyz;
                }
                artificial_light += light_contribution;
            }
        }
    }

#ifdef SS_SHADOW_ONE
    if (lin_depth < 30.0 && lum(brightest_light_contribution) > 0.0) {
        vec2 hit_uv;
        vec3 hit_point;

        float occlusion = 0.0;
        const vec3 light_dir_vs = (g_shrd_data.view_from_world * vec4(normalize(brightest_light_pos - P), 0.0)).xyz;
        if (IntersectRay(pos_vs, light_dir_vs, hash, hit_uv, hit_point)) {
            const float shadow_vis = textureLod(g_albedo_tex, hit_uv, 0.0).w;
            if (shadow_vis > 0.5) {
                occlusion = 1.0;
            }
        }

        // view distance falloff
        occlusion *= saturate(30.0 - lin_depth);
        // shadow fadeout
        occlusion *= saturate(10.0 * (MAX_TRACE_DIST - distance(hit_point, pos_vs)));
        // fix shadow terminator
        occlusion *= saturate(abs(8.0 * dot(N, normalize(brightest_light_pos - P))));
        // substract occluded portion of light
        artificial_light -= occlusion * brightest_light_contribution;
    }
#endif

    vec3 final_color = vec3(0.0);

    if (dot(g_shrd_data.sun_col.xyz, g_shrd_data.sun_col.xyz) > 0.0 && g_shrd_data.sun_dir.y > 0.0) {
        const vec3 sun_color = textureLod(g_sun_shadow_tex, norm_uvs, 0.0).xyz;
        if (hsum(sun_color) > 0.0) {
            final_color += sun_color * EvaluateSunLight_LTC(g_shrd_data.sun_col.xyz, g_shrd_data.sun_dir.xyz, g_shrd_data.sun_dir.w, P, I, N, lobe_masks, g_ltc_luts,
-                                                           sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
        }
    }

    const vec2 px_uvs = (vec2(icoord) + 0.5) * g_shrd_data.ren_res.zw;
    const vec4 gi_fetch = textureLod(g_gi_tex, px_uvs, 0.0);
    vec3 gi_contribution = lobe_masks.diffuse_mul * gi_fetch.xyz / g_shrd_data.cam_pos_and_exp.w;
    gi_contribution *= base_color * g_ltc[gl_LocalInvocationIndex].diff_t2.x;
    // Recover small details lost during denoising
    gi_contribution *= pow(saturate(2.0 * gi_fetch.w), 0.2);

    final_color += artificial_light;
    final_color += gi_contribution;

    imageStore(g_out_color_img, icoord, vec4(compress_hdr(final_color, g_shrd_data.cam_pos_and_exp.w), 0.0));
}
