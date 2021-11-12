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
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D diff_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D norm_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D spec_texture;
layout(binding = REN_MAT_TEX3_SLOT) uniform sampler2D sss_texture;
layout(binding = REN_MAT_TEX4_SLOT) uniform sampler2D norm_detail_texture;
#endif // BINDLESS_TEXTURES
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow shadow_texture;
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D decals_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D ao_texture;
layout(binding = REN_ENV_TEX_SLOT) uniform mediump samplerCubeArray env_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform mediump samplerBuffer lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;
layout(binding = REN_CONE_RT_LUT_SLOT) uniform lowp sampler2D cone_rt_lut;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

LAYOUT(location = 0) in highp vec3 aVertexPos_;
LAYOUT(location = 1) in mediump vec3 aVertexUVAndCurvature_;
LAYOUT(location = 2) in mediump vec3 aVertexNormal_;
LAYOUT(location = 3) in mediump vec3 aVertexTangent_;
LAYOUT(location = 4) in highp vec3 aVertexShUVs_[4];
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 8) in flat TEX_HANDLE diff_texture;
    LAYOUT(location = 9) in flat TEX_HANDLE norm_texture;
    LAYOUT(location = 10) in flat TEX_HANDLE spec_texture;
    LAYOUT(location = 11) in flat TEX_HANDLE sss_texture;
    LAYOUT(location = 12) in flat TEX_HANDLE norm_detail_texture;
#endif // BINDLESS_TEXTURES
LAYOUT(location = 13) in flat vec4 material_params;

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main(void) {
    highp float lin_depth = shrd_data.uClipInfo[0] / (gl_FragCoord.z * (shrd_data.uClipInfo[1] - shrd_data.uClipInfo[2]) + shrd_data.uClipInfo[2]);
    highp float k = log2(lin_depth / shrd_data.uClipInfo[1]) / shrd_data.uClipInfo[3];
    int slice = int(floor(k * float(REN_GRID_RES_Z)));

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / int(shrd_data.uResAndFRes.y)) * REN_GRID_RES_X + ix * REN_GRID_RES_X / int(shrd_data.uResAndFRes.x);

    highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    vec3 albedo_color = texture(SAMPLER2D(diff_texture), aVertexUVAndCurvature_.xy).rgb;

    vec2 duv_dx = dFdx(aVertexUVAndCurvature_.xy), duv_dy = dFdy(aVertexUVAndCurvature_.xy);
    vec3 normal_color = texture(SAMPLER2D(norm_texture), aVertexUVAndCurvature_.xy).wyz;
    vec3 normal_detail_color = texture(SAMPLER2D(norm_detail_texture), aVertexUVAndCurvature_.xy * material_params.w).wyz;

    normal_color.xy += normal_detail_color.xy;

    const vec3 lod_offsets = { 3.0, 2.0, 1.0 };
    const vec3 der_muls = material_params.xyz;//{ 8.0, 4.0, 2.0 };

    vec3 normal_color_r = textureGrad(SAMPLER2D(norm_texture), aVertexUVAndCurvature_.xy,
                                      der_muls.x * dFdx(aVertexUVAndCurvature_.xy), der_muls.x * dFdy(aVertexUVAndCurvature_.xy)).wyz;
    vec3 normal_color_g = textureGrad(SAMPLER2D(norm_texture), aVertexUVAndCurvature_.xy,
                                      der_muls.y * dFdx(aVertexUVAndCurvature_.xy), der_muls.y * dFdy(aVertexUVAndCurvature_.xy)).wyz;
    vec3 normal_color_b = textureGrad(SAMPLER2D(norm_texture), aVertexUVAndCurvature_.xy,
                                      der_muls.z * dFdx(aVertexUVAndCurvature_.xy), der_muls.z * dFdy(aVertexUVAndCurvature_.xy)).wyz;

    /*float lod = textureQueryLod(SAMPLER2D(norm_texture), aVertexUVAndCurvature_.xy).x;
    vec3 normal_color_r = textureLod(SAMPLER2D(norm_texture), aVertexUVAndCurvature_.xy, lod + lod_offsets.x).wyz;
    vec3 normal_color_g = textureLod(SAMPLER2D(norm_texture), aVertexUVAndCurvature_.xy, lod + lod_offsets.y).wyz;
    vec3 normal_color_b = textureLod(SAMPLER2D(norm_texture), aVertexUVAndCurvature_.xy, lod + lod_offsets.z).wyz;*/

    vec4 spec_color = texture(SAMPLER2D(spec_texture), aVertexUVAndCurvature_.xy);

    vec3 dp_dx = dFdx(aVertexPos_);
    vec3 dp_dy = dFdy(aVertexPos_);

    /*for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.x; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int di = int(bitfieldExtract(item_data, 12, 12));

        mat4 de_proj;
        de_proj[0] = texelFetch(decals_buffer, di * 6 + 0);
        de_proj[1] = texelFetch(decals_buffer, di * 6 + 1);
        de_proj[2] = texelFetch(decals_buffer, di * 6 + 2);
        de_proj[3] = vec4(0.0, 0.0, 0.0, 1.0);
        de_proj = transpose(de_proj);

        vec4 pp = de_proj * vec4(aVertexPos_, 1.0);
        pp /= pp[3];

        vec3 app = abs(pp.xyz);
        vec2 uvs = pp.xy * 0.5 + 0.5;

        vec2 duv_dx = 0.5 * (de_proj * vec4(dp_dx, 0.0)).xy;
        vec2 duv_dy = 0.5 * (de_proj * vec4(dp_dy, 0.0)).xy;

        if (app.x < 1.0 && app.y < 1.0 && app.z < 1.0) {
            vec4 diff_uvs_tr = texelFetch(decals_buffer, di * 6 + 3);
            float decal_influence = 0.0;

            if (diff_uvs_tr.z > 0.0) {
                vec2 diff_uvs = diff_uvs_tr.xy + diff_uvs_tr.zw * uvs;

                vec2 _duv_dx = diff_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = diff_uvs_tr.zw * duv_dy;

                vec4 decal_diff = textureGrad(decals_texture, diff_uvs, _duv_dx, _duv_dy);
                decal_influence = decal_diff.a;
                albedo_color = mix(albedo_color, decal_diff.rgb, decal_influence);
            }

            vec4 norm_uvs_tr = texelFetch(decals_buffer, di * 6 + 4);

            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;

                vec2 _duv_dx = 2.0 * norm_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = 2.0 * norm_uvs_tr.zw * duv_dy;

                vec3 decal_norm = textureGrad(decals_texture, norm_uvs, _duv_dx, _duv_dy).wyz;
                normal_color = mix(normal_color, decal_norm, decal_influence);
            }

            vec4 spec_uvs_tr = texelFetch(decals_buffer, di * 6 + 5);

            if (spec_uvs_tr.z > 0.0) {
                vec2 spec_uvs = spec_uvs_tr.xy + spec_uvs_tr.zw * uvs;

                vec2 _duv_dx = spec_uvs_tr.zw * duv_dx;
                vec2 _duv_dy = spec_uvs_tr.zw * duv_dy;

                vec4 decal_spec = textureGrad(decals_texture, spec_uvs, _duv_dx, _duv_dy);
                spec_color = mix(spec_color, decal_spec, decal_influence);
            }
        }
    }*/

    vec3 normal = vec3(normal_color.xy - vec2(1.0), normal_color.z * 2.0 - 1.0);//normal_color * 2.0 - 1.0;
    normal = normalize(mat3(aVertexTangent_, cross(aVertexNormal_, aVertexTangent_), aVertexNormal_) * normal);

    vec3 normal_r = normalize(mat3(aVertexTangent_, cross(aVertexNormal_, aVertexTangent_), aVertexNormal_) * (normal_color_r * 2.0 - 1.0));
    vec3 normal_g = normalize(mat3(aVertexTangent_, cross(aVertexNormal_, aVertexTangent_), aVertexNormal_) * (normal_color_g * 2.0 - 1.0));
    vec3 normal_b = normalize(mat3(aVertexTangent_, cross(aVertexNormal_, aVertexTangent_), aVertexNormal_) * (normal_color_b * 2.0 - 1.0));

    float curvature = clamp(aVertexUVAndCurvature_.z, 0.0, 1.0);

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(lights_buffer, li * 3 + 0);
        highp vec4 col_and_index = texelFetch(lights_buffer, li * 3 + 1);
        vec4 dir_and_spot = texelFetch(lights_buffer, li * 3 + 2);

        vec3 L = pos_and_radius.xyz - aVertexPos_;
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
                vec4 reg_tr = shrd_data.uShadowMapRegions[shadowreg_index].transform;

                highp vec4 pp = shrd_data.uShadowMapRegions[shadowreg_index].clip_from_world * vec4(aVertexPos_, 1.0);
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
                atten *= SampleShadowPCF5x5(shadow_texture, pp.xyz);
            }

#if 0
            float _dot1 = dot(L, normal);
            vec3 l_diffuse = texture(SAMPLER2D(sss_texture), vec2(_dot1 * 0.5 + 0.5, curvature)).rgb;

            additional_light += col_and_index.xyz * atten * l_diffuse * smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
#else
            float _dot_r = dot(L, normal_r);
            float _dot_g = dot(L, normal_g);
            float _dot_b = dot(L, normal_b);
            float l_diffuse_r = texture(SAMPLER2D(sss_texture), vec2(_dot_r * 0.5 + 0.5, curvature)).r;
            float l_diffuse_g = texture(SAMPLER2D(sss_texture), vec2(_dot_g * 0.5 + 0.5, curvature)).g;
            float l_diffuse_b = texture(SAMPLER2D(sss_texture), vec2(_dot_b * 0.5 + 0.5, curvature)).b;

            additional_light += col_and_index.xyz * atten * vec3(l_diffuse_r, l_diffuse_g, l_diffuse_b) * smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
#endif
        }
    }

    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(items_buffer, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(shrd_data.uProbes[pi].pos_and_radius.xyz, aVertexPos_);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / shrd_data.uProbes[pi].pos_and_radius.w);

#if 0
        indirect_col += fade * EvalSHIrradiance_NonLinear(normal,
                                                          shrd_data.uProbes[pi].sh_coeffs[0],
                                                          shrd_data.uProbes[pi].sh_coeffs[1],
                                                          shrd_data.uProbes[pi].sh_coeffs[2]);
#else
        indirect_col.r += fade * EvalSHIrradiance_NonLinear(normal_r,
                                                            shrd_data.uProbes[pi].sh_coeffs[0],
                                                            shrd_data.uProbes[pi].sh_coeffs[1],
                                                            shrd_data.uProbes[pi].sh_coeffs[2]).r;
        indirect_col.g += fade * EvalSHIrradiance_NonLinear(normal_g,
                                                            shrd_data.uProbes[pi].sh_coeffs[0],
                                                            shrd_data.uProbes[pi].sh_coeffs[1],
                                                            shrd_data.uProbes[pi].sh_coeffs[2]).g;
        indirect_col.b += fade * EvalSHIrradiance_NonLinear(normal_b,
                                                            shrd_data.uProbes[pi].sh_coeffs[0],
                                                            shrd_data.uProbes[pi].sh_coeffs[1],
                                                            shrd_data.uProbes[pi].sh_coeffs[2]).b;
#endif

        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(indirect_col, vec3(0.0));

    float N_dot_L = dot(normal, shrd_data.uSunDir.xyz);
    float lambert = clamp(N_dot_L, 0.0, 1.0);

    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, shadow_texture, aVertexShUVs_);
    }

    vec3 sun_diffuse = texture(SAMPLER2D(sss_texture), vec2(N_dot_L * 0.5 + 0.5, curvature)).rgb;

    vec2 ao_uvs = vec2(ix, iy) / shrd_data.uResAndFRes.zw;
    float ambient_occlusion = textureLod(ao_texture, ao_uvs, 0.0).r;
    vec3 diff_color = albedo_color * (shrd_data.uSunCol.xyz * sun_diffuse * visibility + ambient_occlusion * indirect_col + additional_light);

    vec3 view_ray_ws = normalize(shrd_data.uCamPosAndGamma.xyz - aVertexPos_);
    float N_dot_V = clamp(dot(normal, view_ray_ws), 0.0, 1.0);

    vec3 kD = 1.0 - FresnelSchlickRoughness(N_dot_V, spec_color.xyz, spec_color.a);

    outColor = vec4(diff_color * kD, 1.0);
    outNormal = PackNormalAndRoughness(normal, spec_color.w);
    outSpecular = spec_color;

    //outColor.rgb *= 0.00001;
    //outColor.rgb += vec3(curvature);
}
