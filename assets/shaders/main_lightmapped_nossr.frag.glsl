#version 320 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif
//#extension GL_EXT_control_flow_attributes : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
    precision mediump sampler2DShadow;
#else
    #define lowp
    #define mediump
    #define highp
#endif

#include "internal/_fs_common.glsl"
#include "internal/_texturing.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(BINDLESS_TEXTURES)
layout(binding = BIND_MAT_TEX0) uniform sampler2D g_diff_tex;
layout(binding = BIND_MAT_TEX1) uniform sampler2D g_norm_tex;
layout(binding = BIND_MAT_TEX2) uniform sampler2D g_spec_tex;
#endif // BINDLESS_TEXTURES
layout(binding = BIND_SHAD_TEX) uniform sampler2DShadow g_shadow_tex;
layout(binding = BIND_LMAP_SH) uniform sampler2D g_lm_indirect_sh_texture[4];
layout(binding = BIND_DECAL_TEX) uniform sampler2D g_decals_tex;
layout(binding = BIND_SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = BIND_ENV_TEX) uniform mediump samplerCubeArray g_env_tex;
layout(binding = BIND_LIGHT_BUF) uniform highp samplerBuffer g_lights_buf;
layout(binding = BIND_DECAL_BUF) uniform mediump samplerBuffer g_decals_buf;
layout(binding = BIND_CELLS_BUF) uniform highp usamplerBuffer g_cells_buf;
layout(binding = BIND_ITEMS_BUF) uniform highp usamplerBuffer g_items_buf;
layout(binding = BIND_CONE_RT_LUT) uniform lowp sampler2D g_cone_rt_lut;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(location = 0) in highp vec3 g_vtx_pos;
layout(location = 1) in mediump vec4 g_vtx_uvs;
layout(location = 2) in mediump vec3 g_vtx_normal;
layout(location = 3) in mediump vec3 g_vtx_tangent;
layout(location = 4) in highp vec4 g_vtx_sh_uvs0;
layout(location = 5) in highp vec4 g_vtx_sh_uvs1;
layout(location = 6) in highp vec4 g_vtx_sh_uvs2;
#if defined(BINDLESS_TEXTURES)
layout(location = 7) in flat TEX_HANDLE g_diff_tex;
layout(location = 8) in flat TEX_HANDLE g_norm_tex;
layout(location = 9) in flat TEX_HANDLE g_spec_tex;
#endif // BINDLESS_TEXTURES

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;
layout(location = LOC_OUT_NORM) out vec4 g_out_normal;
layout(location = LOC_OUT_SPEC) out vec4 g_out_specular;

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);
    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                          bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                          bitfieldExtract(cell_data.y, 8, 8));

    vec3 diff_color = SRGBToLinear(YCoCg_to_RGB(texture(SAMPLER2D(g_diff_tex), g_vtx_uvs.xy)));
    vec3 norm_color = texture(SAMPLER2D(g_norm_tex), g_vtx_uvs.xy).wyz;
    vec4 spec_color = texture(SAMPLER2D(g_spec_tex), g_vtx_uvs.xy);

    vec3 dp_dx = dFdx(g_vtx_pos);
    vec3 dp_dy = dFdy(g_vtx_pos);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.x; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int di = int(bitfieldExtract(item_data, 12, 12));

        mat4 de_proj;
        de_proj[0] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 0);
        de_proj[1] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 1);
        de_proj[2] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 2);
        de_proj[3] = vec4(0.0, 0.0, 0.0, 1.0);
        de_proj = transpose(de_proj);

        vec4 pp = de_proj * vec4(g_vtx_pos, 1.0);
        pp /= pp[3];

        vec3 app = abs(pp.xyz);
        vec2 uvs = pp.xy * 0.5 + 0.5;

        vec2 duv_dx = 0.5 * (de_proj * vec4(dp_dx, 0.0)).xy;
        vec2 duv_dy = 0.5 * (de_proj * vec4(dp_dy, 0.0)).xy;

        /*[[branch]]*/ if (app.x < 1.0 && app.y < 1.0 && app.z < 1.0) {
            float decal_influence = 1.0;
            vec4 mask_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 3);
            if (mask_uvs_tr.z > 0.0) {
                vec2 mask_uvs = mask_uvs_tr.xy + mask_uvs_tr.zw * uvs;
                decal_influence = textureGrad(g_decals_tex, mask_uvs, mask_uvs_tr.zw * duv_dx, mask_uvs_tr.zw * duv_dy).r;
            }

            vec4 diff_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 4);
            if (diff_uvs_tr.z > 0.0) {
                vec2 diff_uvs = diff_uvs_tr.xy + diff_uvs_tr.zw * uvs;
                vec3 decal_diff = YCoCg_to_RGB(textureGrad(g_decals_tex, diff_uvs, diff_uvs_tr.zw * duv_dx, diff_uvs_tr.zw * duv_dy));
                diff_color = mix(diff_color, SRGBToLinear(decal_diff), decal_influence);
            }

            vec4 norm_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 5);
            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;
                vec3 decal_norm = textureGrad(g_decals_tex, norm_uvs, norm_uvs_tr.zw * duv_dx, norm_uvs_tr.zw * duv_dy).wyz;
                norm_color = mix(norm_color, decal_norm, decal_influence);
            }

            vec4 spec_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 6);
            if (spec_uvs_tr.z > 0.0) {
                vec2 spec_uvs = spec_uvs_tr.xy + spec_uvs_tr.zw * uvs;
                vec4 decal_spec = textureGrad(g_decals_tex, spec_uvs, spec_uvs_tr.zw * duv_dx, spec_uvs_tr.zw * duv_dy);
                spec_color = mix(spec_color, decal_spec, decal_influence);
            }
        }
    }

    vec3 normal = norm_color * 2.0 - 1.0;
    normal = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent, g_vtx_normal) * normal);

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 pos_and_radius = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 0);
        highp vec4 col_and_index = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 2);

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
            /*[[branch]]*/ if (shadowreg_index != -1) {
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

            additional_light += col_and_index.xyz * atten *
                smoothstep(dir_and_spot.w, dir_and_spot.w + 0.2, _dot2);
        }
    }

    vec3 sh_l_00 = RGBMDecode(texture(g_lm_indirect_sh_texture[0], g_vtx_uvs.zw));
    vec3 sh_l_10 = 2.0 * texture(g_lm_indirect_sh_texture[1], g_vtx_uvs.zw).rgb - vec3(1.0);
    vec3 sh_l_11 = 2.0 * texture(g_lm_indirect_sh_texture[2], g_vtx_uvs.zw).rgb - vec3(1.0);
    vec3 sh_l_12 = 2.0 * texture(g_lm_indirect_sh_texture[3], g_vtx_uvs.zw).rgb - vec3(1.0);

    vec3 cone_origin_ws = g_vtx_pos;
    // use green channel of L1 band as cone tracing direction
    vec3 cone_dir_ws = normalize(vec3(sh_l_12.g, sh_l_10.g, sh_l_11.g));

    float cone_occlusion = 1.0;
    float sph_occlusion = 1.0;

    highp uint ecount = bitfieldExtract(cell_data.y, 16, 8);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + ecount; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).y;
        int ei = int(bitfieldExtract(item_data, 0, 8));

        vec4 pos_and_radius = g_shrd_data.ellipsoids[ei].pos_and_radius;
        vec4 axis_and_perp = g_shrd_data.ellipsoids[ei].axis_and_perp;

#if 1
        float axis_len = length(axis_and_perp.xyz);

        mat3 sph_ls;
        sph_ls[0] = vec3(0.0);
        sph_ls[0][floatBitsToInt(axis_and_perp.w)] = 1.0;
        sph_ls[1] = axis_and_perp.xyz;// / axis_len;
        sph_ls[2] = normalize(cross(sph_ls[0], sph_ls[1]));
        sph_ls[0] = normalize(cross(sph_ls[1], sph_ls[2]));
        sph_ls = inverse(sph_ls);

        vec3 cone_origin_ls = sph_ls * (cone_origin_ws.xyz - pos_and_radius.xyz);
        vec3 cone_dir_ls = sph_ls * cone_dir_ws;

        //cone_origin_ls[1] /= axis_len;
        //cone_dir_ls[1] /= axis_len;
        cone_dir_ls = normalize(cone_dir_ls);

        vec3 dir = -cone_origin_ls;
        float dist = length(dir);

        float sin_omega = pos_and_radius.w / sqrt(pos_and_radius.w * pos_and_radius.w + dist * dist);
        float cos_phi = dot(dir, cone_dir_ls) / dist;

        cone_occlusion *= textureLod(g_cone_rt_lut, vec2(cos_phi, sin_omega), 0.0).r;
        sph_occlusion *= clamp(1.0 - (pos_and_radius.w / dist) * (pos_and_radius.w / dist), 0.0, 1.0);
#else
        vec3 dir = pos_and_radius.xyz - cone_origin_ws;
        float dist = length(dir);

        float sin_omega = pos_and_radius.w / sqrt(pos_and_radius.w * pos_and_radius.w +
                                                  dist * dist);
        float cos_phi = dot(dir, cone_dir_ws) / dist;

        cone_occlusion *= textureLod(g_cone_rt_lut, vec2(cos_phi, sin_omega), 0.0).g;
        sph_occlusion *= clamp(1.0 - (pos_and_radius.w / dist) * (pos_and_radius.w / dist), 0.0, 1.0);
#endif
    }

    // squared to compensate gamma
    cone_occlusion *= cone_occlusion;

    vec3 indirect_col = sph_occlusion * EvalSHIrradiance(cone_occlusion * normal, sh_l_00, sh_l_10, sh_l_11, sh_l_12);

    float lambert = clamp(dot(normal, g_shrd_data.sun_dir.xyz), 0.0, 1.0);
    float visibility = 0.0;
    /*[[branch]]*/ if (lambert > 0.00001) {
        visibility = GetSunVisibility(lin_depth, g_shadow_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)));
    }

    vec2 ao_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;
    float ambient_occlusion = textureLod(g_ao_tex, ao_uvs, 0.0).r;
    vec3 diffuse_color = diff_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
                                       ambient_occlusion * ambient_occlusion * indirect_col +
                                       additional_light);

    vec3 view_ray_ws = normalize(g_shrd_data.cam_pos_and_exp.xyz - g_vtx_pos);
    float N_dot_V = clamp(dot(normal, view_ray_ws), 0.0, 1.0);

    vec3 kD = 1.0 - FresnelSchlickRoughness(N_dot_V, spec_color.xyz, spec_color.a);

    g_out_color = vec4(diffuse_color * kD, 1.0);
    g_out_normal = PackNormalAndRoughness(normal, spec_color.w);
    g_out_specular = spec_color;
}
