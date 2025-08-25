#version 430 core
#extension GL_ARB_texture_multisample : enable

#include "_fs_common.glsl"
#include "rt_common.glsl"
#include "principled_common.glsl"
#include "blit_ssr_compose_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
layout(binding = SPEC_TEX_SLOT) uniform usampler2D g_spec_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = LTC_LUTS_TEX_SLOT) uniform sampler2D g_ltc_luts;

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

void main() {
    ivec2 icoord = ivec2(gl_FragCoord.xy);
    vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    const float depth = texelFetch(g_depth_tex, icoord, 0).x;
    const float linear_depth  = LinearizeDepth(depth, g_shrd_data.clip_info);

    const vec4 pos_cs = vec4(2.0 * norm_uvs - 1.0, depth, 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec3 base_color = texelFetch(g_albedo_tex, icoord, 0).xyz;
    const uint packed_mat_params = texelFetch(g_spec_tex, icoord, 0).x;
    const vec4 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0).x);

    const vec3 P = pos_ws;
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

    const float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;
    const ltc_params_t ltc = SampleLTC_Params(g_ltc_luts, N_dot_V, roughness, clearcoat_roughness2);

    vec4 refl = texelFetch(g_refl_tex, icoord, 0);
    //refl.w *= GetHitDistanceNormalization(linear_depth, roughness);

    // Try to use specular occlusion a bit (totally adhoc and fake)
    refl.xyz *= mix(saturate(16.0 * refl.w), 1.0, saturate(0.5 - roughness + metallic));

    vec3 final_color = vec3(0.0);

    if ((lobe_masks.bits & LOBE_SPECULAR_BIT) != 0) {
        final_color += refl.xyz * (approx_spec_col * ltc.spec_t2.x + (1.0 - approx_spec_col) * ltc.spec_t2.y);
    }

    /*if ((lobe_masks.bits & LOBE_CLEARCOAT_BIT) != 0) {
        float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
        float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
        float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

        // Approximation of FH (using shading normal)
        float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);

        vec3 approx_clearcoat_col = vec3(mix(0.04, 1.0, clearcoat_FN));

        vec3 kS = FresnelSchlickRoughness(N_dot_V, approx_clearcoat_col, clearcoat_roughness2);
        vec2 brdf = texture(g_brdf_lut_tex, vec2(N_dot_V, clearcoat_roughness2)).xy;
        final_color += 0.25 * (kS * brdf.x + brdf.y) * refl_color;
    }*/

    const vec3 tonemapped = final_color / (final_color + 1.0);

    g_out_color = vec4(final_color /* (kS * brdf.x + brdf.y)*/, lum(tonemapped) * (1.0 - roughness));
}
