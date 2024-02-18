#version 320 es
#extension GL_ARB_texture_multisample : enable
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "_principled.glsl"
#include "blit_ssr_compose_new_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
layout(binding = SPEC_TEX_SLOT) uniform highp usampler2D g_spec_tex;
layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform highp sampler2D g_norm_tex;
layout(binding = REFL_TEX_SLOT) uniform highp sampler2D g_refl_tex;
layout(binding = BRDF_TEX_SLOT) uniform sampler2D g_brdf_lut_tex;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    ivec2 icoord = ivec2(gl_FragCoord.xy);
    vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    highp float depth = texelFetch(g_depth_tex, icoord, 0).r;

    vec4 pos_cs = vec4(norm_uvs, depth, 1.0);
#if defined(VULKAN)
    pos_cs.xy = 2.0 * pos_cs.xy - 1.0;
    pos_cs.y = -pos_cs.y;
#else // VULKAN
    pos_cs.xyz = 2.0 * pos_cs.xyz - 1.0;
#endif // VULKAN

    vec4 pos_ws = g_shrd_data.inv_view_proj_no_translation * pos_cs;
    pos_ws /= pos_ws.w;
    pos_ws.xyz += g_shrd_data.cam_pos_and_gamma.xyz;

    vec3 base_color = texelFetch(g_albedo_tex, icoord, 0).rgb;
    uint packed_mat_params = texelFetch(g_spec_tex, icoord, 0).r;
    vec4 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0));

    vec3 P = pos_ws.xyz;
    vec3 I = normalize(g_shrd_data.cam_pos_and_gamma.xyz - P);
    vec3 N = normal.xyz;
    float N_dot_V = saturate(dot(N, I));

    vec3 tint_color = vec3(0.0);

    const float base_color_lum = lum(base_color);
    if (base_color_lum > 0.0) {
        tint_color = base_color / base_color_lum;
    }

    vec4 mat_params0, mat_params1;
    UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);

    float roughness = normal.w;
    float sheen = mat_params0.x;
    float sheen_tint = mat_params0.y;
    float specular = mat_params0.z;
    float specular_tint = mat_params0.w;
    float metallic = mat_params1.x;
    float transmission = mat_params1.y;
    float clearcoat = mat_params1.z;
    float clearcoat_roughness = mat_params1.w;

    vec3 spec_tmp_col = mix(vec3(1.0), tint_color, specular_tint);
    spec_tmp_col = mix(specular * 0.08 * spec_tmp_col, base_color, metallic);

    float spec_ior = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
    float spec_F0 = fresnel_dielectric_cos(1.0, spec_ior);

    // Approximation of FH (using shading normal)
    float FN = (fresnel_dielectric_cos(dot(I, N), spec_ior) - spec_F0) / (1.0 - spec_F0);

    vec3 approx_spec_col = mix(spec_tmp_col, vec3(1.0), FN * (1.0 - roughness));
    float spec_color_lum = lum(approx_spec_col);

    lobe_weights_t lobe_weights = get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat);

    vec3 refl_color = texelFetch(g_refl_tex, icoord, 0).rgb;
    vec3 final_color = vec3(0.0);

    if (lobe_weights.specular > 0.0) {
        vec3 kS = FresnelSchlickRoughness(N_dot_V, approx_spec_col, roughness);
        vec2 brdf = texture(g_brdf_lut_tex, vec2(N_dot_V, roughness)).xy;
        final_color += (kS * brdf.x + brdf.y) * refl_color;
    }

    /*if (lobe_weights.clearcoat > 0.0) {
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

    g_out_color = vec4(final_color /* (kS * brdf.x + brdf.y)*/, 1.0);
}
