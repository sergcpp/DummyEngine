#version 320 es
#extension GL_EXT_control_flow_attributes : require

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "_principled.glsl"
#include "gbuffer_shade_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORMAL_TEX_SLOT) uniform usampler2D g_normal_tex;
layout(binding = SPECULAR_TEX_SLOT) uniform usampler2D g_specular_tex;

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = SHADOW_VAL_TEX_SLOT) uniform sampler2D g_shadow_val_tex;
layout(binding = SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = LIGHT_BUF_SLOT) uniform highp samplerBuffer g_lights_buf;
layout(binding = DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buf;
layout(binding = CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buf;
layout(binding = GI_TEX_SLOT) uniform sampler2D g_gi_tex;
layout(binding = SUN_SHADOW_TEX_SLOT) uniform sampler2D g_sun_shadow_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;
layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(binding = OUT_COLOR_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    const vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    const float depth = texelFetch(g_depth_tex, icoord, 0).r;
    const float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);
    if (lin_depth > 3000.0) {
        return;
    }
    const float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    const int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

    const int ix = int(gl_GlobalInvocationID.x), iy = int(gl_GlobalInvocationID.y);
    const int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    const uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    const uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    const uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    vec4 pos_cs = vec4(norm_uvs, depth, 1.0);
#if defined(VULKAN)
    pos_cs.xy = 2.0 * pos_cs.xy - 1.0;
    pos_cs.y = -pos_cs.y;
#else // VULKAN
    pos_cs.xyz = 2.0 * pos_cs.xyz - 1.0;
#endif // VULKAN

    vec4 pos_ws = g_shrd_data.world_from_clip * pos_cs;
    pos_ws /= pos_ws.w;

    const vec3 base_color = texelFetch(g_albedo_tex, icoord, 0).rgb;
    const vec4 normal = UnpackNormalAndRoughness(texelFetch(g_normal_tex, icoord, 0).r);
    const uint packed_mat_params = texelFetch(g_specular_tex, icoord, 0).r;

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

    const lobe_weights_t lobe_weights = get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat);

    const vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

    const float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
    const float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
    const float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

    // Approximation of FH (using shading normal)
    const float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);
    const vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

    const ltc_params_t ltc = SampleLTC_Params(g_ltc_luts, N_dot_V, roughness, clearcoat_roughness2);

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
    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        const highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        const int li = int(bitfieldExtract(item_data, 0, 12));

        light_item_t litem;
        litem.col_and_type = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 0);
        litem.pos_and_radius = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 1);
        litem.dir_and_spot = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 2);
        litem.u_and_reg = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 3);
        litem.v_and_blend = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 4);

        const bool is_portal = (floatBitsToUint(litem.col_and_type.w) & LIGHT_PORTAL_BIT) != 0;

        lobe_weights_t _lobe_weights = lobe_weights;
        if (is_portal) {
            _lobe_weights.specular *= portals_specular_ltc_weight;
            _lobe_weights.clearcoat *= portals_specular_ltc_weight;
        }
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
            vec3 from_light = normalize(P + 0.1 * (hash - 0.5) * N - litem.pos_and_radius.xyz);
            shadowreg_index += cubemap_face(from_light, litem.dir_and_spot.xyz, normalize(litem.u_and_reg.xyz), normalize(litem.v_and_blend.xyz));
            vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

            vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(P, 1.0);
            pp /= pp.w;

            #if defined(VULKAN)
                pp.xy = pp.xy * 0.5 + vec2(0.5);
            #else // VULKAN
                pp.xyz = pp.xyz * 0.5 + vec3(0.5);
            #endif // VULKAN
            pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
            #if defined(VULKAN)
                pp.y = 1.0 - pp.y;
                reg_tr.y = 1.0 - reg_tr.y;
            #endif // VULKAN

            //light_contribution *= SampleShadowPCF5x5(g_shadow_tex, pp.xyz);

            const vec2 ShadowSizePx = vec2(float(SHADOWMAP_RES), float(SHADOWMAP_RES) / 2.0);
            const float MinShadowRadiusPx = 1.5; // needed to hide blockyness
            const float MaxShadowRadiusPx = 5.0;

            vec2 blocker = BlockerSearch(g_shadow_val_tex, pp.xyz, MaxShadowRadiusPx * pp.z, rotator);
            if (blocker.y > 0.5) {
                blocker.x /= blocker.y;

                float penumbra_size_approx = 2.0 * (ShadowSizePx.x * reg_tr.z) * abs(blocker.x - pp.z) * litem.pos_and_radius.w / blocker.x;
                const float filter_radius_px = clamp(penumbra_size_approx, MinShadowRadiusPx, MaxShadowRadiusPx);
                float visibility = 0.0;
                for (int i = 0; i < 16; ++i) {
                    vec2 uv = pp.xy + filter_radius_px * RotateVector(rotator, g_poisson_disk_16[i] / ShadowSizePx);
                    visibility += textureLod(g_shadow_tex, vec3(uv, pp.z), 0.0);
                }
                light_contribution *= (visibility / 16.0);
            }
        }

        artificial_light += light_contribution;
    }

    //
    // Indirect probes
    //
    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, P);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

        indirect_col += fade * EvalSHIrradiance_NonLinear(normal.xyz, g_shrd_data.probes[pi].sh_coeffs[0],
                                                                      g_shrd_data.probes[pi].sh_coeffs[1],
                                                                      g_shrd_data.probes[pi].sh_coeffs[2]);
        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(1.0 * indirect_col, vec3(0.0));

    vec2 px_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;

    //float lambert = clamp(dot(normal.xyz, g_shrd_data.sun_dir.xyz), 0.0, 1.0);

    vec3 final_color = vec3(0.0);//base_color * g_shrd_data.sun_col.xyz * lambert * sun_visibility;

    if (dot(g_shrd_data.sun_col.xyz, g_shrd_data.sun_col.xyz) > 0.0) {
        const float sun_visibility = texelFetch(g_sun_shadow_tex, ivec2(ix, iy), 0).r;
        if (sun_visibility > 0.0) {
            final_color += sun_visibility * EvaluateSunLight(g_shrd_data.sun_col.xyz, g_shrd_data.sun_dir.xyz, g_shrd_data.sun_dir.w, P, I, N, lobe_weights, ltc, g_ltc_luts,
                                                             sheen, base_color, sheen_color, approx_spec_col, approx_clearcoat_col);
        }
    }

    final_color += artificial_light;
    final_color += lobe_weights.diffuse_mul * base_color * decompress_hdr(textureLod(g_gi_tex, px_uvs, 0.0).rgb);

    const float sun_visibility = texelFetch(g_sun_shadow_tex, ivec2(ix, iy), 0).r;
    imageStore(g_out_color_img, icoord, vec4(compress_hdr(final_color), 0.0));
}
