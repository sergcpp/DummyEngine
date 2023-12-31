#version 310 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "_ltc.glsl"
#include "_principled.glsl"
#include "gbuffer_shade_interface.h"

#define LIGHT_ATTEN_CUTOFF 0.004

#define ENABLE_SPHERE_LIGHT 1
#define ENABLE_RECT_LIGHT 1
#define ENABLE_DISK_LIGHT 1
#define ENABLE_LINE_LIGHT 1

#define ENABLE_DIFFUSE 1
#define ENABLE_SHEEN 1
#define ENABLE_SPECULAR 1
#define ENABLE_CLEARCOAT 1

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    UniformParams : $ubUnifParamLoc
PERM @HQ_HDR
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORMAL_TEX_SLOT) uniform sampler2D g_normal_tex;
layout(binding = SPECULAR_TEX_SLOT) uniform usampler2D g_specular_tex;

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = LIGHT_BUF_SLOT) uniform highp samplerBuffer g_lights_buf;
layout(binding = DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buf;
layout(binding = CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buf;
layout(binding = GI_TEX_SLOT) uniform sampler2D g_gi_tex;
layout(binding = SUN_SHADOW_TEX_SLOT) uniform sampler2D g_sun_shadow_tex;
layout(binding = LTC_DIFF_LUT_TEX_SLOT) uniform sampler2D g_ltc_diff_lut[2];
layout(binding = LTC_SHEEN_LUT_TEX_SLOT) uniform sampler2D g_ltc_sheen_lut[2];
layout(binding = LTC_SPEC_LUT_TEX_SLOT) uniform sampler2D g_ltc_spec_lut[2];
layout(binding = LTC_COAT_LUT_TEX_SLOT) uniform sampler2D g_ltc_coat_lut[2];

#ifdef HQ_HDR
layout(binding = OUT_COLOR_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;
#else
layout(binding = OUT_COLOR_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_color_img;
#endif

vec3 EvaluateLightSource() {
    return vec3(0.0);
}

int cubemap_face(vec3 dir) {
    if (abs(dir.x) >= abs(dir.y) && abs(dir.x) >= abs(dir.z)) {
        return dir.x > 0.0 ? 0 : 1;
    } else if (abs(dir.y) >= abs(dir.z)) {
        return dir.y > 0.0 ? 2 : 3;
    }
    return dir.z > 0.0 ? 4 : 5;
}

int cubemap_face(vec3 _dir, vec3 f, vec3 u, vec3 v) {
    vec3 dir = vec3(-dot(_dir, f), dot(_dir, u), dot(_dir, v));
    if (abs(dir.x) >= abs(dir.y) && abs(dir.x) >= abs(dir.z)) {
        return dir.x > 0.0 ? 0 : 1;
    } else if (abs(dir.y) >= abs(dir.z)) {
        return dir.y > 0.0 ? 2 : 3;
    }
    return dir.z > 0.0 ? 4 : 5;
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    highp float depth = texelFetch(g_depth_tex, icoord, 0).r;
    highp float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);
    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(REN_GRID_RES_Z)), 0, REN_GRID_RES_Z - 1);

    int ix = int(gl_GlobalInvocationID.x), iy = int(gl_GlobalInvocationID.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

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
    vec4 normal = UnpackNormalAndRoughness(texelFetch(g_normal_tex, icoord, 0));
    uint packed_mat_params = texelFetch(g_specular_tex, icoord, 0).r;

    //
    // Artificial lights
    //
    vec3 additional_light = vec3(0.0);

    vec3 V = normalize(g_shrd_data.cam_pos_and_gamma.xyz - pos_ws.xyz);
    float N_dot_V = saturate(dot(normal.xyz, V));

    vec3 I = V;
    vec3 N = normal.xyz;

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

    float diffuse_weight, specular_weight, clearcoat_weight, refraction_weight;
    get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat,
                     diffuse_weight, specular_weight, clearcoat_weight, refraction_weight);

    vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

    float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
    float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
    float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

    // Approximation of FH (using shading normal)
    float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);

    vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

    //
    // Fetch LTC data
    //

    vec2 ltc_uv = LTC_Coords(N_dot_V, roughness);

    vec4 diff_t1 = textureLod(g_ltc_diff_lut[0], ltc_uv, 0.0);
    vec2 diff_t2 = textureLod(g_ltc_diff_lut[1], ltc_uv, 0.0).xy;

    vec4 sheen_t1 = textureLod(g_ltc_sheen_lut[0], ltc_uv, 0.0);
    vec2 sheen_t2 = textureLod(g_ltc_sheen_lut[1], ltc_uv, 0.0).xy;

    vec4 spec_t1 = textureLod(g_ltc_spec_lut[0], ltc_uv, 0.0);
    vec2 spec_t2 = textureLod(g_ltc_spec_lut[1], ltc_uv, 0.0).xy;

    vec2 coat_ltc_uv = LTC_Coords(N_dot_V, clearcoat_roughness2);
    vec4 coat_t1 = textureLod(g_ltc_coat_lut[0], coat_ltc_uv, 0.0);
    vec2 coat_t2 = textureLod(g_ltc_coat_lut[1], coat_ltc_uv, 0.0).xy;

    //
    // Evaluate lights
    //

    vec2 debug_uvs = vec2(0.0), _unused;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 col_and_type = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 0);
        vec4 pos_and_radius = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 2);

        vec4 u_and_reg = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 3);
        vec4 v_and_unused = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 4);

        const bool TwoSided = false;

        int shadowreg_index = floatBitsToInt(u_and_reg.w);
        if (shadowreg_index != -1) {
            shadowreg_index += cubemap_face(normalize(pos_ws.xyz - pos_and_radius.xyz), dir_and_spot.xyz, normalize(u_and_reg.xyz), normalize(v_and_unused.xyz));
            vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

            vec4 pp = g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(pos_ws.xyz, 1.0);
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

            col_and_type.xyz *= SampleShadowPCF5x5(g_shadow_tex, pp.xyz);
        }

        int type = floatBitsToInt(col_and_type.w);
        if (type == LIGHT_TYPE_SPHERE && ENABLE_SPHERE_LIGHT != 0) {
            vec3 lp = pos_and_radius.xyz;

            // TODO: simplify this!
            vec3 to = normalize(pos_ws.xyz - lp);
            vec3 u = normalize(V - to * dot(V, to));
            vec3 v = cross(to, u);

            u *= pos_and_radius.w;
            v *= pos_and_radius.w;

            vec3 points[4];
            points[0] = lp + u + v;
            points[1] = lp + u - v;
            points[2] = lp - u - v;
            points[3] = lp - u + v;

            if (diffuse_weight > 0.0 && ENABLE_DIFFUSE != 0) {
                vec3 dcol = base_color;

                vec3 diff = LTC_Evaluate_Disk(g_ltc_diff_lut[1], N, V, pos_ws.xyz, diff_t1, points, TwoSided);
                diff *= dcol * diff_t2.x;// + (1.0 - dcol) * diff_t2.y;

                if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                    vec3 _sheen = LTC_Evaluate_Disk(g_ltc_sheen_lut[1], N, V, pos_ws.xyz, sheen_t1, points, TwoSided);
                    diff += _sheen * (sheen_color * sheen_t2.x + (1.0 - sheen_color) * sheen_t2.y);
                }

                additional_light += col_and_type.xyz * diff / M_PI;
            }

            if (specular_weight > 0.0 && ENABLE_SPECULAR != 0) {
                vec3 scol = approx_spec_col;

                vec3 spec = LTC_Evaluate_Disk(g_ltc_spec_lut[1], N, V, pos_ws.xyz, spec_t1, points, TwoSided);
                spec *= scol * spec_t2.x + (1.0 - scol) * spec_t2.y;

                additional_light += col_and_type.xyz * spec / M_PI;
            }

            if (clearcoat_weight > 0.0 && ENABLE_CLEARCOAT != 0) {
                vec3 ccol = approx_clearcoat_col;

                vec3 coat = LTC_Evaluate_Disk(g_ltc_coat_lut[1], N, V, pos_ws.xyz, coat_t1, points, TwoSided);
                coat *= ccol * coat_t2.x + (1.0 - ccol) * coat_t2.y;

                additional_light += 0.25 * col_and_type.xyz * coat / M_PI;
            }
        } else if (type == LIGHT_TYPE_RECT && ENABLE_RECT_LIGHT != 0) {
            vec3 lp = pos_and_radius.xyz;

            vec3 points[4];
            points[0] = lp + u_and_reg.xyz + v_and_unused.xyz;
            points[1] = lp + u_and_reg.xyz - v_and_unused.xyz;
            points[2] = lp - u_and_reg.xyz - v_and_unused.xyz;
            points[3] = lp - u_and_reg.xyz + v_and_unused.xyz;

            if (diffuse_weight > 0.0 && ENABLE_DIFFUSE != 0) {
                vec3 dcol = base_color;

                vec3 diff = LTC_Evaluate_Rect(g_ltc_diff_lut[1], N, V, pos_ws.xyz, diff_t1, points, TwoSided);
                diff *= dcol * diff_t2.x;// + (1.0 - dcol) * diff_t2.y;

                if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                    vec3 _sheen = LTC_Evaluate_Rect(g_ltc_sheen_lut[1], N, V, pos_ws.xyz, sheen_t1, points, TwoSided);
                    diff += _sheen * (sheen_color * sheen_t2.x + (1.0 - sheen_color) * sheen_t2.y);
                }

                additional_light += col_and_type.xyz * diff / 4.0;
            }

            if (specular_weight > 0.0 && ENABLE_SPECULAR != 0) {
                vec3 scol = approx_spec_col;

                vec3 spec = LTC_Evaluate_Rect(g_ltc_spec_lut[1], N, V, pos_ws.xyz, spec_t1, points, TwoSided);
                spec *= scol * spec_t2.x + (1.0 - scol) * spec_t2.y;

                additional_light += col_and_type.xyz * spec / 4.0;
            }

            if (clearcoat_weight > 0.0 && ENABLE_CLEARCOAT != 0) {
                vec3 ccol = approx_clearcoat_col;

                vec3 coat = LTC_Evaluate_Rect(g_ltc_coat_lut[1], N, V, pos_ws.xyz, coat_t1, points, TwoSided);
                coat *= ccol * coat_t2.x + (1.0 - ccol) * coat_t2.y;

                additional_light += 0.25 * col_and_type.xyz * coat / 4.0;
            }
        } else if (type == LIGHT_TYPE_DISK && ENABLE_DISK_LIGHT != 0) {
            vec3 lp = pos_and_radius.xyz;

            vec3 points[4];
            points[0] = lp + u_and_reg.xyz + v_and_unused.xyz;
            points[1] = lp + u_and_reg.xyz - v_and_unused.xyz;
            points[2] = lp - u_and_reg.xyz - v_and_unused.xyz;
            points[3] = lp - u_and_reg.xyz + v_and_unused.xyz;

            if (diffuse_weight > 0.0 && ENABLE_DIFFUSE != 0) {
                vec3 dcol = base_color;

                vec3 diff = LTC_Evaluate_Disk(g_ltc_diff_lut[1], N, V, pos_ws.xyz, diff_t1, points, TwoSided);
                diff *= dcol * diff_t2.x;// + (1.0 - dcol) * diff_t2.y;

                if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                    vec3 _sheen = LTC_Evaluate_Disk(g_ltc_sheen_lut[1], N, V, pos_ws.xyz, sheen_t1, points, TwoSided);
                    diff += _sheen * (sheen_color * sheen_t2.x + (1.0 - sheen_color) * sheen_t2.y);
                }

                additional_light += col_and_type.xyz * diff / 4.0;
            }

            if (specular_weight > 0.0 && ENABLE_SPECULAR != 0) {
                vec3 scol = approx_spec_col;

                vec3 spec = LTC_Evaluate_Disk(g_ltc_spec_lut[1], N, V, pos_ws.xyz, spec_t1, points, TwoSided);
                spec *= scol * spec_t2.x + (1.0 - scol) * spec_t2.y;

                additional_light += col_and_type.xyz * spec / 4.0;
            }

            if (clearcoat_weight > 0.0 && ENABLE_CLEARCOAT != 0) {
                vec3 ccol = approx_clearcoat_col;

                vec3 coat = LTC_Evaluate_Disk(g_ltc_coat_lut[1], N, V, pos_ws.xyz, coat_t1, points, TwoSided);
                coat *= ccol * coat_t2.x + (1.0 - ccol) * coat_t2.y;

                additional_light += 0.25 * col_and_type.xyz * coat / 4.0;
            }
        } else if (type == LIGHT_TYPE_LINE && ENABLE_LINE_LIGHT != 0) {
            vec3 lp = pos_and_radius.xyz;

            vec3 points[2];
            points[0] = lp + v_and_unused.xyz;
            points[1] = lp - v_and_unused.xyz;

            if (diffuse_weight > 0.0 && ENABLE_DIFFUSE != 0) {
                vec3 dcol = base_color;

                vec3 diff = LTC_Evaluate_Line(N, V, pos_ws.xyz, diff_t1, points, 0.01);
                diff *= dcol * diff_t2.x;// + (1.0 - dcol) * diff_t2.y;

                if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                    vec3 _sheen = LTC_Evaluate_Line(N, V, pos_ws.xyz, sheen_t1, points, 0.01);
                    diff += _sheen * (sheen_color * sheen_t2.x + (1.0 - sheen_color) * sheen_t2.y);
                }

                additional_light += col_and_type.xyz * diff;
            }

            if (specular_weight > 0.0 && ENABLE_SPECULAR != 0) {
                vec3 scol = approx_spec_col;

                vec3 spec = LTC_Evaluate_Line(N, V, pos_ws.xyz, spec_t1, points, 0.01);
                spec *= scol * spec_t2.x + (1.0 - scol) * spec_t2.y;

                additional_light += col_and_type.xyz * spec;
            }

            if (clearcoat_weight > 0.0 && ENABLE_CLEARCOAT != 0) {
                vec3 ccol = approx_clearcoat_col;

                vec3 coat = LTC_Evaluate_Line(N, V, pos_ws.xyz, coat_t1, points, 0.01);
                coat *= ccol * coat_t2.x + (1.0 - ccol) * coat_t2.y;

                additional_light += 0.25 * col_and_type.xyz * coat / 4.0;
            }
        }
    }

    //
    // Indirect probes
    //
    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, pos_ws.xyz);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

        indirect_col += fade * EvalSHIrradiance_NonLinear(normal.xyz, g_shrd_data.probes[pi].sh_coeffs[0],
                                                                      g_shrd_data.probes[pi].sh_coeffs[1],
                                                                      g_shrd_data.probes[pi].sh_coeffs[2]);
        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(1.0 * indirect_col, vec3(0.0));

    vec2 px_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;

    float lambert = clamp(dot(normal.xyz, g_shrd_data.sun_dir.xyz), 0.0, 1.0);
    float visibility = 0.0;
#if 0
    if (lambert > 0.00001) {
        vec4 g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2;

        const vec2 offsets[4] = vec2[4](
            vec2(0.0, 0.0),
            vec2(0.25, 0.0),
            vec2(0.0, 0.5),
            vec2(0.25, 0.5)
        );

        /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
            vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * pos_ws).xyz;
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

        visibility = GetSunVisibility(lin_depth, g_shadow_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)));
    }
#else
    if (lambert > 0.00001) {
        visibility = texelFetch(g_sun_shadow_tex, ivec2(ix, iy), 0).r;
    }
#endif

    vec4 gi = textureLod(g_gi_tex, px_uvs, 0.0);
    //vec3 gi = vec3(textureLod(g_gi_tex, px_uvs, 0.0).a * 0.01);
    //float ao = (gi.a * 0.01);
    vec3 diffuse_color = base_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
                                       gi.rgb /*+ ao * indirect_col +*/) + additional_light;

    //float ambient_occlusion = textureLod(g_ao_tex, px_uvs, 0.0).r;
    //vec3 diffuse_color = base_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
    //                                   ambient_occlusion * ambient_occlusion * indirect_col +
    //                                   additional_light);

    imageStore(g_out_color_img, icoord, vec4(diffuse_color, 0.0));
    //imageStore(g_out_color_img, icoord, vec4(N * 0.5 + vec3(0.5), 0.0));
}
