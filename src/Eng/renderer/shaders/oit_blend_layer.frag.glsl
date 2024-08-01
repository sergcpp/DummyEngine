#version 430 core
#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_fs_common.glsl"
#include "texturing_common.glsl"
#include "principled_common.glsl"
#include "gi_cache_common.glsl"
#include "taa_common.glsl"
#include "oit_blend_layer_interface.h"

#pragma multi_compile _ GI_CACHE
#pragma multi_compile _ SPECULAR
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#if defined(NO_BINDLESS)
    layout(binding = BIND_MAT_TEX0) uniform sampler2D g_base_tex;
    layout(binding = BIND_MAT_TEX1) uniform sampler2D g_norm_tex;
    layout(binding = BIND_MAT_TEX2) uniform sampler2D g_roug_tex;
    layout(binding = BIND_MAT_TEX3) uniform sampler2D g_metl_tex;
    layout(binding = BIND_MAT_TEX4) uniform sampler2D g_alpha_tex;
#endif // !NO_BINDLESS

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = LIGHT_BUF_SLOT) uniform samplerBuffer g_lights_buf;
layout(binding = DECAL_BUF_SLOT) uniform samplerBuffer g_decals_buf;
layout(binding = CELLS_BUF_SLOT) uniform usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform usamplerBuffer g_items_buf;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;
layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;
layout(binding = OIT_DEPTH_BUF_SLOT) uniform usamplerBuffer g_oit_depth_buf;

layout(binding = BACK_COLOR_TEX_SLOT) uniform sampler2D g_back_color_tex;
layout(binding = BACK_DEPTH_TEX_SLOT) uniform sampler2D g_back_depth_tex;

#ifdef SPECULAR
    layout(binding = SPEC_TEX_SLOT) uniform sampler2D g_specular_tex;
#endif

#ifdef GI_CACHE
    layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
    layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
    layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;
#endif

layout(location = 0) in vec3 g_vtx_pos;
layout(location = 1) in vec2 g_vtx_uvs;
layout(location = 2) in vec3 g_vtx_normal;
layout(location = 3) in vec3 g_vtx_tangent;
#if !defined(NO_BINDLESS)
    layout(location = 4) in flat TEX_HANDLE g_base_tex;
    layout(location = 5) in flat TEX_HANDLE g_norm_tex;
    layout(location = 6) in flat TEX_HANDLE g_roug_tex;
    layout(location = 7) in flat TEX_HANDLE g_metl_tex;
    layout(location = 8) in flat TEX_HANDLE g_alpha_tex;
#endif // !NO_BINDLESS
layout(location = 9) in flat vec4 g_base_color;
layout(location = 10) in flat vec4 g_mat_params0;
layout(location = 11) in flat vec4 g_mat_params1;
layout(location = 12) in flat vec4 g_mat_params2;

layout(location = 0) out vec4 g_out_color;

void main() {
    const float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);
    const float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    const int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

    const ivec2 icoord = ivec2(gl_FragCoord.xy);
    const vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    const int cell_index = GetCellIndex(icoord.x, icoord.y, slice, g_shrd_data.res_and_fres.xy);
    const uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    const uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                          bitfieldExtract(cell_data.x, 24, 8));
    const uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                          bitfieldExtract(cell_data.y, 8, 8));

    vec3 base_color = YCoCg_to_RGB(texture(SAMPLER2D(g_base_tex), g_vtx_uvs));
    const vec2 norm_color = texture(SAMPLER2D(g_norm_tex), g_vtx_uvs).xy;
    const float roug_color = texture(SAMPLER2D(g_roug_tex), g_vtx_uvs).r;
    const float metl_color = texture(SAMPLER2D(g_metl_tex), g_vtx_uvs).r;
    const float alpha = (1.0 - g_mat_params2.x) * texture(SAMPLER2D(g_alpha_tex), g_vtx_uvs).r;

    // TODO: Apply decals here (?)

    base_color = SRGBToLinear(base_color) * g_base_color.rgb;

    vec3 normal;
    normal.xy = norm_color * 2.0 - 1.0;
    normal.z = sqrt(saturate(1.0 - normal.x * normal.x - normal.y * normal.y));
    normal = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent, g_vtx_normal) * normal);
    if (!gl_FrontFacing) {
        normal = -normal;
    }

    //
    // Artificial lights
    //

    const vec3 P = g_vtx_pos;
    const vec3 I = normalize(g_shrd_data.cam_pos_and_exp.xyz - P);
    const vec3 N = normal.xyz;
    const float N_dot_V = saturate(dot(N, I));

    vec3 tint_color = vec3(0.0);

    const float base_color_lum = lum(base_color);
    if (base_color_lum > 0.0) {
        tint_color = base_color / base_color_lum;
    }

    const float roughness = roug_color * g_base_color.w;
    const float sheen = g_mat_params0.x;
    const float sheen_tint = g_mat_params0.y;
    const float specular = g_mat_params0.z;
    const float specular_tint = g_mat_params0.w;
    const float metallic = g_mat_params1.x * metl_color;
    const float transmission = g_mat_params1.y;
    const float clearcoat = g_mat_params1.z;
    const float clearcoat_roughness = g_mat_params1.w;
    const float ior = g_mat_params2.y;

    vec3 spec_tmp_col = mix(vec3(1.0), tint_color, specular_tint);
    spec_tmp_col = mix(specular * 0.08 * spec_tmp_col, base_color, metallic);

    const float spec_ior = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
    const float spec_F0 = fresnel_dielectric_cos(1.0, spec_ior);

    // Approximation of FH (using shading normal)
    const float FN = (fresnel_dielectric_cos(dot(I, N), spec_ior) - spec_F0) / (1.0 - spec_F0);

    vec3 approx_spec_col = mix(spec_tmp_col, vec3(1.0), FN * (1.0 - roughness));
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

    const float portals_specular_ltc_weight = smoothstep(0.0, 0.25, roughness);

    if (lobe_weights.refraction > 0.0) {
        approx_spec_col = vec3(1.0);
    }

    vec3 artificial_light = vec3(0.0);
    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        const uint item_data = texelFetch(g_items_buf, int(i)).x;
        const int li = int(bitfieldExtract(item_data, 0, 12));

        const light_item_t litem = FetchLightItem(g_lights_buf, li);
        const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;
        const bool is_diffuse = (floatBitsToUint(litem.col_and_type.w) & LIGHT_DIFFUSE_BIT) != 0;
        const bool is_specular = (floatBitsToUint(litem.col_and_type.w) & LIGHT_SPECULAR_BIT) != 0;

        lobe_weights_t _lobe_weights = lobe_weights;
        if (is_portal) {
            _lobe_weights.specular *= portals_specular_ltc_weight;
            _lobe_weights.clearcoat *= portals_specular_ltc_weight;
        }
        [[flatten]] if (!is_diffuse) _lobe_weights.diffuse = 0.0;
        [[flatten]] if (!is_specular) _lobe_weights.specular = _lobe_weights.clearcoat = 0.0;
        vec3 light_contribution = EvaluateLightSource(litem, P, I, N, _lobe_weights, ltc, g_ltc_luts,
                                                      sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
        if (all(equal(light_contribution, vec3(0.0)))) {
            continue;
        }
        if (is_portal) {
            // Sample environment to create slight color variation
            const vec3 rotated_dir = rotate_xz(normalize(litem.pos_and_radius.xyz - P), g_shrd_data.env_col.w);
            light_contribution *= textureLod(g_env_tex, rotated_dir, g_shrd_data.ambient_hack.w - 2.0).rgb;
        }

        int shadowreg_index = floatBitsToInt(litem.u_and_reg.w);
        [[dont_flatten]] if (shadowreg_index != -1) {
            vec3 from_light = normalize(P - litem.pos_and_radius.xyz);
            shadowreg_index += cubemap_face(from_light, litem.dir_and_spot.xyz, normalize(litem.u_and_reg.xyz), normalize(litem.v_and_blend.xyz));
            vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

            vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(P, 1.0);
            pp /= pp.w;

            pp.xy = pp.xy * 0.5 + 0.5;
            pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
            #if defined(VULKAN)
                pp.y = 1.0 - pp.y;
            #endif // VULKAN

            light_contribution *= SampleShadowPCF5x5(g_shadow_tex, pp.xyz);
        }

        artificial_light += light_contribution;
    }

    vec3 sun_color = vec3(0.0);

    const float dot_N_L = saturate(dot(normal, g_shrd_data.sun_dir.xyz));
    if (dot_N_L > -0.01) {
        const vec2 offsets[4] = vec2[4](
            vec2(0.0, 0.0),
            vec2(0.25, 0.0),
            vec2(0.0, 0.5),
            vec2(0.25, 0.5)
        );

        const vec2 shadow_offsets = get_shadow_offsets(dot_N_L);
        vec3 pos_ws_biased = P;
        pos_ws_biased += 0.001 * shadow_offsets.x * normal;
        pos_ws_biased += 0.003 * shadow_offsets.y * g_shrd_data.sun_dir.xyz;

        vec4 g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2;
        [[unroll]] for (int i = 0; i < 4; i++) {
            vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * vec4(pos_ws_biased, 1.0)).xyz;
            shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
            shadow_uvs.xy *= vec2(0.25, 0.5);
            shadow_uvs.xy += offsets[i];
    #if defined(VULKAN)
            shadow_uvs.y = 1.0 - shadow_uvs.y;
    #endif // VULKAN

            g_vtx_sh_uvs0[i] = shadow_uvs[0];
            g_vtx_sh_uvs1[i] = shadow_uvs[1];
            g_vtx_sh_uvs2[i] = shadow_uvs[2];
        }

        const float sun_visibility =  GetSunVisibility(lin_depth, g_shadow_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)));
        if (sun_visibility > 0.0) {
            sun_color = sun_visibility * EvaluateSunLight(g_shrd_data.sun_col.xyz, g_shrd_data.sun_dir.xyz, g_shrd_data.sun_dir.w, P, I, N, lobe_weights, ltc, g_ltc_luts,
                                                          sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
        }
    }

    vec3 gi_color = vec3(0.0);
#ifdef GI_CACHE
    for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
        const float weight = get_volume_blend_weight(P, g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
        if (weight > 0.0) {
            if (lobe_weights.diffuse > 0.0) {
                gi_color += weight * get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(normal.xyz, I, g_shrd_data.probe_volumes[i].spacing.xyz), normal.xyz,
                                                            g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
                if (weight < 1.0 && i < PROBE_VOLUMES_COUNT - 1) {
                    gi_color += (1.0 - weight) * get_volume_irradiance_sep(i + 1, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(normal.xyz, I, g_shrd_data.probe_volumes[i + 1].spacing.xyz), normal.xyz,
                                                                        g_shrd_data.probe_volumes[i + 1].scroll.xyz, g_shrd_data.probe_volumes[i + 1].origin.xyz, g_shrd_data.probe_volumes[i + 1].spacing.xyz);
                }
                gi_color *= (lobe_weights.diffuse_mul / M_PI);
                gi_color *= base_color * ltc.diff_t2.x;
            }
#ifndef SPECULAR
            if (lobe_weights.specular > 0.0) {
                const vec3 refl_dir = reflect(I, N);
                vec3 avg_radiance = get_volume_irradiance_sep(i, g_irradiance_tex, g_distance_tex, g_offset_tex, P, get_surface_bias(I, g_shrd_data.probe_volumes[i].spacing.xyz), refl_dir,
                                                                g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
                avg_radiance *= approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y;
                gi_color += (1.0 / M_PI) * avg_radiance;
            }
#endif
            break;
        }
    }
#endif

    vec3 final_color = vec3(0.0);
    final_color += artificial_light;
    final_color += sun_color;
    final_color += gi_color;

    float fresnel = 1.0;
    [[dont_flatten]] if (lobe_weights.refraction > 0.0) {
        const float eta = 1.0 / ior;//gl_FrontFacing ? (1.0 / ior) : (ior / 1.0);
        fresnel = fresnel_dielectric_cos(dot(I, N), 1.0 / eta);

        const float back_depth = texelFetch(g_back_depth_tex, icoord, 0).x;
        const float back_lin_depth = LinearizeDepth(back_depth, g_shrd_data.clip_info);

        const vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(N, 0.0)).xyz);

        vec2 uvs_offset = -normal_vs.xy;
    #if defined(VULKAN)
        uvs_offset.y = -uvs_offset.y;
    #endif

        float k = 4.0 * saturate(ior - 1.0) * (back_lin_depth - lin_depth) / back_lin_depth;
        k *= 0.5 - abs(norm_uvs.x - 0.5);
        k *= 0.5 - abs(norm_uvs.y - 0.5);

        const vec3 background_col = textureLod(g_back_color_tex, norm_uvs + k * uvs_offset, 0.0).xyz;
        final_color *= fresnel;
        final_color += (1.0 - fresnel) * base_color * decompress_hdr(background_col);
    }

#ifdef SPECULAR
    //vec4 refl = textureLod(g_specular_tex, norm_uvs, 0.0);
    vec4 refl = SampleTextureCatmullRom(g_specular_tex, norm_uvs, vec2(g_shrd_data.ires_and_ifres.xy / 2));
    refl.xyz = decompress_hdr(refl.xyz / max(refl.w, 0.001));
    if (lobe_weights.specular > 0.0 || lobe_weights.refraction > 0.0) {
        final_color += fresnel * refl.xyz * (approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y);
    }
#endif

    g_out_color = vec4(compress_hdr(final_color), alpha);
}
