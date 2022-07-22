#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"
#include "internal/_texturing.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D g_diff_tex;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D g_norm_tex;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D g_spec_tex;
layout(binding = REN_MAT_TEX3_SLOT) uniform sampler2D g_sss_tex;
layout(binding = REN_MAT_TEX4_SLOT) uniform sampler2D g_norm_detail_tex;
#endif // BINDLESS_TEXTURES
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = REN_LMAP_SH_SLOT) uniform sampler2D g_lm_indirect_sh_texture[4];
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D g_decals_tex;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray g_env_tex;
layout(binding = REN_LIGHT_BUF_SLOT) uniform highp samplerBuffer g_lights_buf;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buf;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buf;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buf;
layout(binding = REN_CONE_RT_LUT_SLOT) uniform lowp sampler2D g_cone_rt_lut;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT(location = 0) in highp vec3 g_vtx_pos;
LAYOUT(location = 1) in mediump vec3 aVertexUVAndCurvature_;
LAYOUT(location = 2) in mediump vec3 g_vtx_normal;
LAYOUT(location = 3) in mediump vec3 g_vtx_tangent;
LAYOUT(location = 4) in highp vec3 g_vtx_sh_uvs[4];
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 8) in flat TEX_HANDLE g_diff_tex;
    LAYOUT(location = 9) in flat TEX_HANDLE g_norm_tex;
    LAYOUT(location = 10) in flat TEX_HANDLE g_spec_tex;
    LAYOUT(location = 11) in flat TEX_HANDLE g_sss_tex;
    LAYOUT(location = 12) in flat TEX_HANDLE g_norm_detail_tex;
#endif // BINDLESS_TEXTURES
LAYOUT(location = 13) in flat vec4 material_params;

layout(location = REN_OUT_COLOR_INDEX) out vec4 g_out_color;
layout(location = REN_OUT_NORM_INDEX) out vec4 g_out_normal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 g_out_specular;

void main(void) {
    highp float lin_depth = g_shrd_data.clip_info[0] / (gl_FragCoord.z * (g_shrd_data.clip_info[1] - g_shrd_data.clip_info[2]) + g_shrd_data.clip_info[2]);
    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(REN_GRID_RES_Z)), 0, REN_GRID_RES_Z - 1);

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / int(g_shrd_data.res_and_fres.y)) * REN_GRID_RES_X + ix * REN_GRID_RES_X / int(g_shrd_data.res_and_fres.x);

    highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    vec3 albedo_color = SRGBToLinear(YCoCg_to_RGB(texture(SAMPLER2D(g_diff_tex), aVertexUVAndCurvature_.xy)));

    vec2 duv_dx = dFdx(aVertexUVAndCurvature_.xy), duv_dy = dFdy(aVertexUVAndCurvature_.xy);
    vec3 normal_color = texture(SAMPLER2D(g_norm_tex), aVertexUVAndCurvature_.xy).wyz;
    vec3 normal_detail_color = texture(SAMPLER2D(g_norm_detail_tex), aVertexUVAndCurvature_.xy * material_params.w).wyz;

    normal_color.xy += normal_detail_color.xy;

    const vec3 lod_offsets = { 3.0, 2.0, 1.0 };
    const vec3 der_muls = material_params.xyz;//{ 8.0, 4.0, 2.0 };

    vec3 normal_color_r = textureGrad(SAMPLER2D(g_norm_tex), aVertexUVAndCurvature_.xy,
                                      der_muls.x * dFdx(aVertexUVAndCurvature_.xy), der_muls.x * dFdy(aVertexUVAndCurvature_.xy)).wyz;
    vec3 normal_color_g = textureGrad(SAMPLER2D(g_norm_tex), aVertexUVAndCurvature_.xy,
                                      der_muls.y * dFdx(aVertexUVAndCurvature_.xy), der_muls.y * dFdy(aVertexUVAndCurvature_.xy)).wyz;
    vec3 normal_color_b = textureGrad(SAMPLER2D(g_norm_tex), aVertexUVAndCurvature_.xy,
                                      der_muls.z * dFdx(aVertexUVAndCurvature_.xy), der_muls.z * dFdy(aVertexUVAndCurvature_.xy)).wyz;

    /*float lod = textureQueryLod(SAMPLER2D(g_norm_tex), aVertexUVAndCurvature_.xy).x;
    vec3 normal_color_r = textureLod(SAMPLER2D(g_norm_tex), aVertexUVAndCurvature_.xy, lod + lod_offsets.x).wyz;
    vec3 normal_color_g = textureLod(SAMPLER2D(g_norm_tex), aVertexUVAndCurvature_.xy, lod + lod_offsets.y).wyz;
    vec3 normal_color_b = textureLod(SAMPLER2D(g_norm_tex), aVertexUVAndCurvature_.xy, lod + lod_offsets.z).wyz;*/

    vec4 spec_color = texture(SAMPLER2D(g_spec_tex), aVertexUVAndCurvature_.xy);

    vec3 normal = vec3(normal_color.xy - vec2(1.0), normal_color.z * 2.0 - 1.0);//normal_color * 2.0 - 1.0;
    normal = normalize(mat3(g_vtx_tangent, cross(g_vtx_normal, g_vtx_tangent), g_vtx_normal) * normal);

    vec3 normal_r = normalize(mat3(g_vtx_tangent, cross(g_vtx_normal, g_vtx_tangent), g_vtx_normal) * (normal_color_r * 2.0 - 1.0));
    vec3 normal_g = normalize(mat3(g_vtx_tangent, cross(g_vtx_normal, g_vtx_tangent), g_vtx_normal) * (normal_color_g * 2.0 - 1.0));
    vec3 normal_b = normalize(mat3(g_vtx_tangent, cross(g_vtx_normal, g_vtx_tangent), g_vtx_normal) * (normal_color_b * 2.0 - 1.0));

    float curvature = clamp(aVertexUVAndCurvature_.z, 0.0, 1.0);

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(g_lights_buf, li * 3 + 0);
        highp vec4 col_and_index = texelFetch(g_lights_buf, li * 3 + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buf, li * 3 + 2);

        vec3 L = pos_and_radius.xyz - g_vtx_pos;
        float dist = length(L);
        float d = max(dist - pos_and_radius.w, 0.0);
        L /= dist;

        highp float denom = d / pos_and_radius.w + 1.0;
        highp float atten = 1.0 / (denom * denom);

        highp float brightness = max(col_and_index.x, max(col_and_index.y, col_and_index.z));

        highp float factor = LIGHT_ATTEN_CUTOFF / brightness;
        atten = (atten - factor) / (1.0 - LIGHT_ATTEN_CUTOFF);
        atten = max(atten, 0.0);

        float _dot2 = dot(L, dir_and_spot.xyz);

        if (_dot2 > dir_and_spot.w && (brightness * atten) > FLT_EPS) {
            int shadowreg_index = floatBitsToInt(col_and_index.w);
            if (shadowreg_index != -1) {
                vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

                highp vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(g_vtx_pos, 1.0);
                pp /= pp.w;

#if defined(VULKAN)
                pp.xy = pp.xy * 0.5 + vec2(0.5);
#else // VULKAN
                pp.xyz = pp.xyz * 0.5 + vec3(0.5);
#endif // VULKAN
                pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;
#if defined(VULKAN)
                pp.y = 1.0 - pp.y;
#endif // VULKAN
                atten *= SampleShadowPCF5x5(g_shadow_tex, pp.xyz);
            }

#if 0
            float _dot1 = dot(L, normal);
            vec3 l_diffuse = texture(SAMPLER2D(g_sss_tex), vec2(_dot1 * 0.5 + 0.5, curvature)).rgb;

            additional_light += col_and_index.xyz * atten * l_diffuse * smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
#else
            float _dot_r = dot(L, normal_r);
            float _dot_g = dot(L, normal_g);
            float _dot_b = dot(L, normal_b);
            float l_diffuse_r = texture(SAMPLER2D(g_sss_tex), vec2(_dot_r * 0.5 + 0.5, curvature)).r;
            float l_diffuse_g = texture(SAMPLER2D(g_sss_tex), vec2(_dot_g * 0.5 + 0.5, curvature)).g;
            float l_diffuse_b = texture(SAMPLER2D(g_sss_tex), vec2(_dot_b * 0.5 + 0.5, curvature)).b;

            additional_light += col_and_index.xyz * atten * vec3(l_diffuse_r, l_diffuse_g, l_diffuse_b) * smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
#endif
        }
    }

    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, g_vtx_pos);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

#if 0
        indirect_col += fade * EvalSHIrradiance_NonLinear(normal,
                                                          g_shrd_data.probes[pi].sh_coeffs[0],
                                                          g_shrd_data.probes[pi].sh_coeffs[1],
                                                          g_shrd_data.probes[pi].sh_coeffs[2]);
#else
        indirect_col.r += fade * EvalSHIrradiance_NonLinear(normal_r,
                                                            g_shrd_data.probes[pi].sh_coeffs[0],
                                                            g_shrd_data.probes[pi].sh_coeffs[1],
                                                            g_shrd_data.probes[pi].sh_coeffs[2]).r;
        indirect_col.g += fade * EvalSHIrradiance_NonLinear(normal_g,
                                                            g_shrd_data.probes[pi].sh_coeffs[0],
                                                            g_shrd_data.probes[pi].sh_coeffs[1],
                                                            g_shrd_data.probes[pi].sh_coeffs[2]).g;
        indirect_col.b += fade * EvalSHIrradiance_NonLinear(normal_b,
                                                            g_shrd_data.probes[pi].sh_coeffs[0],
                                                            g_shrd_data.probes[pi].sh_coeffs[1],
                                                            g_shrd_data.probes[pi].sh_coeffs[2]).b;
#endif

        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(indirect_col, vec3(0.0));

    float N_dot_L = dot(normal, g_shrd_data.sun_dir.xyz);
    float lambert = clamp(N_dot_L, 0.0, 1.0);

    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, g_shadow_tex, g_vtx_sh_uvs);
    }

    vec3 sun_diffuse = texture(SAMPLER2D(g_sss_tex), vec2(N_dot_L * 0.5 + 0.5, curvature)).rgb;

    vec2 ao_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;
    float ambient_occlusion = textureLod(g_ao_tex, ao_uvs, 0.0).r;
    vec3 diff_color = albedo_color * (g_shrd_data.sun_col.xyz * sun_diffuse * visibility + ambient_occlusion * indirect_col + additional_light);

    vec3 view_ray_ws = normalize(g_shrd_data.cam_pos_and_gamma.xyz - g_vtx_pos);
    float N_dot_V = clamp(dot(normal, view_ray_ws), 0.0, 1.0);

    vec3 kD = 1.0 - FresnelSchlickRoughness(N_dot_V, spec_color.xyz, spec_color.a);

    g_out_color = vec4(diff_color * kD, 1.0);
    g_out_normal = PackNormalAndRoughness(normal, spec_color.w);
    g_out_specular = spec_color;

    //g_out_color.rgb *= 0.00001;
    //g_out_color.rgb += vec3(curvature);
}
