#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "_texturing.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D g_diff_tex;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D g_norm_tex;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D g_spec_tex;
#endif // BINDLESS_TEXTURES
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D g_decals_tex;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buf;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buf;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buf;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT(location = 0) in highp vec3 g_vtx_pos;
LAYOUT(location = 1) in mediump vec2 g_vtx_uvs;
LAYOUT(location = 2) in mediump vec3 g_vtx_normal;
LAYOUT(location = 3) in mediump vec3 g_vtx_tangent;
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 4) in flat TEX_HANDLE g_diff_tex;
    LAYOUT(location = 5) in flat TEX_HANDLE g_norm_tex;
    LAYOUT(location = 6) in flat TEX_HANDLE g_spec_tex;
#endif // BINDLESS_TEXTURES

layout(location = REN_OUT_ALBEDO_INDEX) out vec4 g_out_albedo;
layout(location = REN_OUT_NORM_INDEX) out vec4 g_out_normal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 g_out_specular;

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);
    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(REN_GRID_RES_Z)), 0, REN_GRID_RES_Z - 1);

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                          bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                          bitfieldExtract(cell_data.y, 8, 8));

    vec3 diff_color = YCoCg_to_RGB(texture(SAMPLER2D(g_diff_tex), g_vtx_uvs));
    vec3 norm_color = texture(SAMPLER2D(g_norm_tex), g_vtx_uvs).wyz;
    vec4 spec_color = texture(SAMPLER2D(g_spec_tex), g_vtx_uvs);

    vec2 duv_dx = dFdx(g_vtx_uvs), duv_dy = dFdy(g_vtx_uvs);
    vec3 dp_dx = dFdx(g_vtx_pos);
    vec3 dp_dy = dFdy(g_vtx_pos);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.x; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int di = int(bitfieldExtract(item_data, 12, 12));

        mat4 de_proj;
        de_proj[0] = texelFetch(g_decals_buf, di * REN_DECALS_BUF_STRIDE + 0);
        de_proj[1] = texelFetch(g_decals_buf, di * REN_DECALS_BUF_STRIDE + 1);
        de_proj[2] = texelFetch(g_decals_buf, di * REN_DECALS_BUF_STRIDE + 2);
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
            vec4 mask_uvs_tr = texelFetch(g_decals_buf, di * REN_DECALS_BUF_STRIDE + 3);
            if (mask_uvs_tr.z > 0.0) {
                vec2 mask_uvs = mask_uvs_tr.xy + mask_uvs_tr.zw * uvs;
                decal_influence = textureGrad(g_decals_tex, mask_uvs, mask_uvs_tr.zw * duv_dx, mask_uvs_tr.zw * duv_dy).r;
            }

            vec4 diff_uvs_tr = texelFetch(g_decals_buf, di * REN_DECALS_BUF_STRIDE + 4);
            if (diff_uvs_tr.z > 0.0) {
                vec2 diff_uvs = diff_uvs_tr.xy + diff_uvs_tr.zw * uvs;
                vec3 decal_diff = YCoCg_to_RGB(textureGrad(g_decals_tex, diff_uvs, diff_uvs_tr.zw * duv_dx, diff_uvs_tr.zw * duv_dy));
                diff_color = mix(diff_color, decal_diff.rgb, decal_influence);
            }

            vec4 norm_uvs_tr = texelFetch(g_decals_buf, di * REN_DECALS_BUF_STRIDE + 5);
            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;
                vec3 decal_norm = textureGrad(g_decals_tex, norm_uvs, norm_uvs_tr.zw * duv_dx, norm_uvs_tr.zw * duv_dy).wyz;
                norm_color = mix(norm_color, decal_norm, decal_influence);
            }

            vec4 spec_uvs_tr = texelFetch(g_decals_buf, di * REN_DECALS_BUF_STRIDE + 6);
            if (spec_uvs_tr.z > 0.0) {
                vec2 spec_uvs = spec_uvs_tr.xy + spec_uvs_tr.zw * uvs;
                vec4 decal_spec = textureGrad(g_decals_tex, spec_uvs, spec_uvs_tr.zw * duv_dx, spec_uvs_tr.zw * duv_dy);
                spec_color = mix(spec_color, decal_spec, decal_influence);
            }
        }
    }

    vec3 normal = norm_color * 2.0 - 1.0;
    normal = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent, g_vtx_normal) * normal);

    // TODO: get rid of explicit srgb conversion
    g_out_albedo = vec4(SRGBToLinear(diff_color), 1.0);
    g_out_normal = PackNormalAndRoughness(normal, spec_color.w);
    g_out_specular = spec_color;
}
