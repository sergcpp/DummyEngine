#version 430 core
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_fs_common.glsl"
#include "texturing_common.glsl"

#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

#define FORCE_FLAT_NORMALS 0
//#define FORCE_ROUGHNESS 0.1
#define FORCE_GREY_ALBEDO 0
//#define FORCE_METALLIC 1.0

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

#if defined(NO_BINDLESS)
    layout(binding = BIND_MAT_TEX0) uniform sampler2D g_base_tex;
    layout(binding = BIND_MAT_TEX1) uniform sampler2D g_norm_tex;
    layout(binding = BIND_MAT_TEX2) uniform sampler2D g_roug_tex;
    layout(binding = BIND_MAT_TEX3) uniform sampler2D g_metl_tex;
#endif // !NO_BINDLESS
layout(binding = BIND_DECAL_TEX) uniform sampler2D g_decals_tex;
layout(binding = BIND_DECAL_BUF) uniform samplerBuffer g_decals_buf;
layout(binding = BIND_CELLS_BUF) uniform usamplerBuffer g_cells_buf;
layout(binding = BIND_ITEMS_BUF) uniform usamplerBuffer g_items_buf;

layout(location = 0) in vec3 g_vtx_pos;
layout(location = 1) in vec2 g_vtx_uvs;
layout(location = 2) in vec3 g_vtx_normal;
layout(location = 3) in vec3 g_vtx_tangent;
#if !defined(NO_BINDLESS)
    layout(location = 4) in flat TEX_HANDLE g_base_tex;
    layout(location = 5) in flat TEX_HANDLE g_norm_tex;
    layout(location = 6) in flat TEX_HANDLE g_roug_tex;
    layout(location = 7) in flat TEX_HANDLE g_metl_tex;
#endif // !NO_BINDLESS
layout(location = 8) in flat vec4 g_base_color;
layout(location = 9) in flat vec4 g_mat_params0;
layout(location = 10) in flat vec4 g_mat_params1;
layout(location = 11) in flat float g_shadow_vis;

layout(location = LOC_OUT_ALBEDO) out vec4 g_out_albedo;
layout(location = LOC_OUT_NORM) out uint g_out_normal;
layout(location = LOC_OUT_SPEC) out uint g_out_specular;

void main() {
    const float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);
    const float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    const int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

    const int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    const int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.ren_res.xy);

    const uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    const uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24),
                                          bitfieldExtract(cell_data.x, 24, 8));
    const uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8),
                                          bitfieldExtract(cell_data.y, 8, 8));

    vec3 base_color = YCoCg_to_RGB(textureBindless(g_base_tex, g_vtx_uvs));
    vec2 norm_color = textureBindless(g_norm_tex, g_vtx_uvs).xy;
    float roug_color = textureBindless(g_roug_tex, g_vtx_uvs).x;
    float metl_color = textureBindless(g_metl_tex, g_vtx_uvs).x;

#if 0 // decals
    vec2 duv_dx = dFdx(g_vtx_uvs), duv_dy = dFdy(g_vtx_uvs);
    const vec3 dp_dx = dFdx(g_vtx_pos);
    const vec3 dp_dy = dFdy(g_vtx_pos);

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.x; i++) {
        uint item_data = texelFetch(g_items_buf, int(i)).x;
        int di = int(bitfieldExtract(item_data, 12, 12));

        mat4 de_proj;
        de_proj[0] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 0);
        de_proj[1] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 1);
        de_proj[2] = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 2);
        de_proj[3] = vec4(0.0, 0.0, 0.0, 1.0);
        de_proj = transpose(de_proj);

        vec4 pp = de_proj * vec4(g_vtx_pos, 1.0);
        pp /= pp.w;

        vec3 app = abs(pp.xyz);
        vec2 uvs = pp.xy * 0.5 + 0.5;

        vec2 duv_dx = 0.5 * (de_proj * vec4(dp_dx, 0.0)).xy;
        vec2 duv_dy = 0.5 * (de_proj * vec4(dp_dy, 0.0)).xy;

        if (app.x < 1.0 && app.y < 1.0 && app.z < 1.0) {
            float decal_influence = 1.0;
            vec4 mask_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 3);
            if (mask_uvs_tr.z > 0.0) {
                vec2 mask_uvs = mask_uvs_tr.xy + mask_uvs_tr.zw * uvs;
                decal_influence = textureGrad(g_decals_tex, mask_uvs, mask_uvs_tr.zw * duv_dx, mask_uvs_tr.zw * duv_dy).x;
            }

            vec4 diff_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 4);
            if (diff_uvs_tr.z > 0.0) {
                vec2 diff_uvs = diff_uvs_tr.xy + diff_uvs_tr.zw * uvs;
                vec3 decal_diff = YCoCg_to_RGB(textureGrad(g_decals_tex, diff_uvs, diff_uvs_tr.zw * duv_dx, diff_uvs_tr.zw * duv_dy));
                base_color = mix(base_color, decal_diff.xyz, decal_influence);
            }

            vec4 norm_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 5);
            if (norm_uvs_tr.z > 0.0) {
                vec2 norm_uvs = norm_uvs_tr.xy + norm_uvs_tr.zw * uvs;
                vec2 decal_norm = textureGrad(g_decals_tex, norm_uvs, norm_uvs_tr.zw * duv_dx, norm_uvs_tr.zw * duv_dy).xy;
                norm_color = mix(norm_color, decal_norm, decal_influence);
            }

            /*vec4 spec_uvs_tr = texelFetch(g_decals_buf, di * DECALS_BUF_STRIDE + 6);
            if (spec_uvs_tr.z > 0.0) {
                vec2 spec_uvs = spec_uvs_tr.xy + spec_uvs_tr.zw * uvs;
                vec4 decal_spec = textureGrad(g_decals_tex, spec_uvs, spec_uvs_tr.zw * duv_dx, spec_uvs_tr.zw * duv_dy);
                roug_color = mix(roug_color, decal_spec, decal_influence);
            }*/
        }
    }
#endif

    vec3 normal;
    normal.xy = norm_color * 2.0 - 1.0;
    normal.z = sqrt(saturate(1.0 - normal.x * normal.x - normal.y * normal.y));
    normal = normalize(mat3(cross(g_vtx_tangent, g_vtx_normal), g_vtx_tangent, g_vtx_normal) * normal);
#if FORCE_FLAT_NORMALS
    normal = g_vtx_normal;
#endif
    if (!gl_FrontFacing) {
        normal = -normal;
    }

    vec2 dither = vec2(0.0);
    if (dot(fwidth(normal), fwidth(normal)) > 1e-7) {
        dither = vec2(Bayer4x4(uvec2(ix, iy), 0), Bayer4x4(uvec2(ix, iy), 1));
        dither = fract(dither + float(floatBitsToUint(g_shrd_data.taa_info.z) & 0xFFu) * GOLDEN_RATIO) - 0.5;
    }

#ifndef FORCE_ROUGHNESS
    const float final_roughness = roug_color * g_base_color.w;
#else
    const float final_roughness = FORCE_ROUGHNESS;
#endif

    // TODO: try to get rid of explicit srgb conversion
#if !FORCE_GREY_ALBEDO
    g_out_albedo = vec4(SRGBToLinear(base_color) * g_base_color.xyz, g_shadow_vis);
#else
    g_out_albedo = vec4(0.5, 0.5, 0.5, g_shadow_vis);
#endif
    g_out_normal = PackNormalAndRoughnessNew(normal, final_roughness, dither);
#ifndef FORCE_METALLIC
    g_out_specular = PackMaterialParams(g_mat_params0, g_mat_params1 * vec4(metl_color, 1.0, 1.0, 1.0));
#else
    g_out_specular = PackMaterialParams(g_mat_params0, g_mat_params1 * vec4(FORCE_METALLIC, 1.0, 1.0, 1.0));
#endif
}
