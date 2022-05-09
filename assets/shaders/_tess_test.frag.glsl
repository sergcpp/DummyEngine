#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture: enable
#endif
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(GL_ARB_bindless_texture)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D g_diff_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D g_norm_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D g_spec_texture;
layout(binding = REN_MAT_TEX3_SLOT) uniform sampler2D g_bump_texture;
#endif // GL_ARB_bindless_texture
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow g_shadow_texture;
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D g_decals_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D g_ao_texture;
layout(binding = REN_BRDF_TEX_SLOT) uniform sampler2D g_brdf_lut_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform highp samplerBuffer g_lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buffer;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 g_vtx_pos;
layout(location = 1) in mediump vec2 g_vtx_uvs;
layout(location = 2) in mediump vec3 g_vtx_normal;
layout(location = 3) in mediump vec3 g_vtx_tangent;
layout(location = 4) in highp vec3 g_vtx_sh_uvs[4];
layout(location = 8) in lowp float g_tex_height;
#if defined(GL_ARB_bindless_texture)
layout(location = 9) in flat uvec2 g_diff_texture;
layout(location = 10) in flat uvec2 g_norm_texture;
layout(location = 11) in flat uvec2 g_spec_texture;
layout(location = 12) in flat uvec2 g_bump_texture;
#endif // GL_ARB_bindless_texture
#else
in highp vec3 g_vtx_pos;
in mediump vec2 g_vtx_uvs;
in mediump vec3 g_vtx_normal;
in mediump vec3 g_vtx_tangent;
in highp vec3 g_vtx_sh_uvs[4];
in lowp float g_tex_height;
#if defined(GL_ARB_bindless_texture)
in flat uvec2 g_diff_texture;
in flat uvec2 g_norm_texture;
in flat uvec2 g_spec_texture;
in flat uvec2 g_bump_texture;
#endif // GL_ARB_bindless_texture
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 g_out_color;
layout(location = REN_OUT_NORM_INDEX) out vec4 g_out_normal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 g_out_specular;

vec2 ReliefParallaxMapping(vec3 dir, vec2 uvs) {
    const float ParallaxScale = 0.008;// * fract(g_shrd_data.transp_params_and_time.w);

    const float MinLayers = 4.0;
    const float MaxLayers = 32.0;
    float layer_count = mix(MaxLayers, MinLayers, clamp(abs(dir.z), 0.0, 1.0));

    float layer_height = 1.0 / layer_count;
    float cur_layer_height = 1.0 - g_tex_height;

    vec2 duvs = ParallaxScale * dir.xy / dir.z / layer_count;
    vec2 cur_uvs = uvs;

    float height = texture(g_bump_texture, cur_uvs).r;

    while (height > cur_layer_height) {
        cur_layer_height += layer_height;
        cur_uvs += duvs;
        height = texture(g_bump_texture, cur_uvs).r;
    }

    duvs = 0.5 * duvs;
    layer_height = 0.5 * layer_height;

    cur_uvs -= duvs;
    cur_layer_height -= layer_height;

    const int BinSearchInterations = 8;
    for (int i = 0; i < BinSearchInterations; i++) {
        duvs = 0.5 * duvs;
        layer_height = 0.5 * layer_height;
        height = texture(g_bump_texture, cur_uvs).r;
        if (height > cur_layer_height) {
            cur_uvs += duvs;
            cur_layer_height += layer_height;
        } else {
            cur_uvs -= duvs;
            cur_layer_height -= layer_height;
        }
    }

    return cur_uvs;
}

vec2 ParallaxOcclusionMapping(vec3 dir, vec2 uvs) {
    const float ParallaxScale = 0.008;

    const float MinLayers = 4.0;
    const float MaxLayers = 16.0;
    float layer_count = mix(MaxLayers, MinLayers, clamp(abs(dir.z), 0.0, 1.0));

    float layer_height = 1.0 / layer_count;
    float cur_layer_height = 0.0;//1.0 - g_tex_height;

    vec2 duvs = ParallaxScale * dir.xy / dir.z / layer_count;
    vec2 cur_uvs = uvs;

    float height = texture(g_bump_texture, cur_uvs).r;

    while (height > cur_layer_height) {
        cur_layer_height += layer_height;
        cur_uvs += duvs;
        height = texture(g_bump_texture, cur_uvs).r;
    }

    vec2 prev_uvs = cur_uvs - duvs;

    float next_height = height - cur_layer_height;
    float prev_height = texture(g_bump_texture, prev_uvs).r - cur_layer_height + layer_height;

    float weight = next_height / (next_height - prev_height);
    vec2 final_uvs = mix(cur_uvs, prev_uvs, weight);

    return final_uvs;
}

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);
    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = int(floor(k * float(REN_GRID_RES_Z)));

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    highp uvec2 cell_data = texelFetch(g_cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                          bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                          bitfieldExtract(cell_data.y, 8, 8));

    vec3 view_ray_ws = normalize(g_shrd_data.cam_pos_and_gamma.xyz - g_vtx_pos);
    vec3 view_ray_ts = vec3(
        dot(view_ray_ws, cross(g_vtx_tangent, g_vtx_normal)),
        -dot(view_ray_ws, g_vtx_tangent),
        dot(view_ray_ws, g_vtx_normal)
    );
    //vec2 modified_uvs = g_vtx_uvs;//ReliefParallaxMapping(view_ray_ts, g_vtx_uvs);
    vec2 modified_uvs = ParallaxOcclusionMapping(view_ray_ts, g_vtx_uvs);

    vec3 albedo_color = texture(g_diff_texture, modified_uvs).rgb;

    vec2 duv_dx = dFdx(modified_uvs), duv_dy = dFdy(modified_uvs);
    vec3 normal_color = texture(g_norm_texture, modified_uvs).wyz;
    vec4 specular_color = texture(g_spec_texture, modified_uvs);

    vec3 dp_dx = dFdx(g_vtx_pos);
    vec3 dp_dy = dFdy(g_vtx_pos);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.x; i++) {
        highp uint item_data = texelFetch(g_items_buffer, int(i)).x;
        int di = int(bitfieldExtract(item_data, 12, 12));

        mat4 de_proj;
        de_proj[0] = texelFetch(g_decals_buffer, di * REN_DECALS_BUF_STRIDE + 0);
        de_proj[1] = texelFetch(g_decals_buffer, di * REN_DECALS_BUF_STRIDE + 1);
        de_proj[2] = texelFetch(g_decals_buffer, di * REN_DECALS_BUF_STRIDE + 2);
        de_proj[3] = vec4(0.0, 0.0, 0.0, 1.0);
        de_proj = transpose(de_proj);

        vec4 pp = de_proj * vec4(g_vtx_pos, 1.0);
        pp /= pp[3];

        vec3 app = abs(pp.xyz);
        vec2 uvs = pp.xy * 0.5 + 0.5;

        vec2 duv_dx = 0.5 * (de_proj * vec4(dp_dx, 0.0)).xy;
        vec2 duv_dy = 0.5 * (de_proj * vec4(dp_dy, 0.0)).xy;

        if (app.x < 1.0 && app.y < 1.0 && app.z < 1.0) {
            float decal_influence = 1.0;
            vec4 mask_uvs_tr = texelFetch(g_decals_buffer, di * REN_DECALS_BUF_STRIDE + 3);
            if (mask_uvs_tr.z > 0.0) {
                vec2 mask_uvs = mask_uvs_tr.xy + mask_uvs_tr.zw * uvs;
                decal_influence = textureGrad(g_decals_texture, mask_uvs, mask_uvs_tr.zw * duv_dx, mask_uvs_tr.zw * duv_dy).r;
            }

            vec4 diff_uvs_tr = texelFetch(g_decals_buffer, di * REN_DECALS_BUF_STRIDE + 4);
            if (diff_uvs_tr.z > 0.0) {
                vec2 diff_uvs = diff_uvs_tr.xy + diff_uvs_tr.zw * uvs;
                vec3 decal_diff = YCoCg_to_RGB(textureGrad(g_decals_texture, diff_uvs, diff_uvs_tr.zw * duv_dx, diff_uvs_tr.zw * duv_dy));
                albedo_color = mix(albedo_color, SRGBToLinear(decal_diff), decal_influence);
            }

            vec4 norm_uvs_tr = texelFetch(g_decals_buffer, di * REN_DECALS_BUF_STRIDE + 5);
            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;
                vec3 decal_norm = textureGrad(g_decals_texture, norm_uvs, norm_uvs_tr.zw * duv_dx, norm_uvs_tr.zw * duv_dy).wyz;
                normal_color = mix(normal_color, decal_norm, decal_influence);
            }

            vec4 spec_uvs_tr = texelFetch(g_decals_buffer, di * REN_DECALS_BUF_STRIDE + 6);
            if (spec_uvs_tr.z > 0.0) {
                vec2 spec_uvs = spec_uvs_tr.xy + spec_uvs_tr.zw * uvs;
                vec4 decal_spec = textureGrad(g_decals_texture, spec_uvs, spec_uvs_tr.zw * duv_dx, spec_uvs_tr.zw * duv_dy);
                specular_color = mix(specular_color, decal_spec, decal_influence);
            }
        }
    }

    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent,
                            g_vtx_normal) * normal);

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buffer, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(g_lights_buffer, li * 3 + 0);
        highp vec4 col_and_index = texelFetch(g_lights_buffer, li * 3 + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buffer, li * 3 + 2);

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

        float _dot1 = clamp(dot(L, normal), 0.0, 1.0);
        float _dot2 = dot(L, dir_and_spot.xyz);

        atten = _dot1 * atten;
        if (_dot2 > dir_and_spot.w && (brightness * atten) > FLT_EPS) {
            int shadowreg_index = floatBitsToInt(col_and_index.w);
            if (shadowreg_index != -1) {
                vec4 reg_tr = g_shrd_data.shadowmap_regions[shadowreg_index].transform;

                highp vec4 pp =
                    g_shrd_data.shadowmap_regions[shadowreg_index].clip_from_world * vec4(g_vtx_pos, 1.0);
                pp /= pp.w;
                pp.xyz = pp.xyz * 0.5 + vec3(0.5);
                pp.xy = reg_tr.xy + pp.xy * reg_tr.zw;

                atten *= SampleShadowPCF5x5(g_shadow_texture, pp.xyz);
            }

            additional_light += col_and_index.xyz * atten *
                smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }

    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buffer, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, g_vtx_pos);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

        indirect_col += fade * EvalSHIrradiance_NonLinear(normal,
                                                          g_shrd_data.probes[pi].sh_coeffs[0],
                                                          g_shrd_data.probes[pi].sh_coeffs[1],
                                                          g_shrd_data.probes[pi].sh_coeffs[2]);
        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(1.0 * indirect_col, vec3(0.0));

    float lambert = clamp(dot(normal, g_shrd_data.sun_dir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, g_shadow_texture, g_vtx_sh_uvs);
    }

    vec2 ao_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;
    float ambient_occlusion = textureLod(g_ao_texture, ao_uvs, 0.0).r;
    vec3 diffuse_color = albedo_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
                                         ambient_occlusion * ambient_occlusion * indirect_col +
                                         additional_light);

    float N_dot_V = clamp(dot(normal, view_ray_ws), 0.0, 1.0);

    vec3 kD = 1.0 - FresnelSchlickRoughness(N_dot_V, specular_color.rgb, specular_color.a);

    g_out_color = vec4(diffuse_color * kD, 1.0);
    g_out_normal = vec4(normal * 0.5 + 0.5, 1.0);
    g_out_specular = specular_color;

    //vec3 view_vec_rec = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent,
    //                            g_vtx_normal) * view_ray_ts);
    //g_out_color.rgb = 0.0001 * g_out_color.rgb + 100.0 * abs(view_vec_rec - view_ray_ws);
}
