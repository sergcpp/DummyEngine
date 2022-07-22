#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"
#include "internal/_texturing.glsl"

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D g_diff_tex;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D g_norm_tex;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D g_spec_tex;
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
LAYOUT(location = 1) in mediump vec2 g_vtx_uvs;
LAYOUT(location = 2) in mediump vec3 g_vtx_normal;
LAYOUT(location = 3) in mediump vec3 g_vtx_tangent;
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 8) in flat TEX_HANDLE g_diff_tex;
    LAYOUT(location = 9) in flat TEX_HANDLE g_norm_tex;
    LAYOUT(location = 10) in flat TEX_HANDLE g_spec_tex;
#endif // BINDLESS_TEXTURES

layout(location = REN_OUT_COLOR_INDEX) out vec4 g_out_color;
layout(location = REN_OUT_NORM_INDEX) out vec4 g_out_normal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 g_out_specular;

void main(void) {
    highp float lin_depth = LinearizeDepth(gl_FragCoord.z, g_shrd_data.clip_info);

    // remapped depth in [-1; 1] range used for moments calculation
    highp float transp_z =
        2.0 * (log(lin_depth) - g_shrd_data.transp_params_and_time[0]) /
            g_shrd_data.transp_params_and_time[1] - 1.0;

    vec3 normal_color = texture(SAMPLER2D(g_norm_tex), g_vtx_uvs).wyz;

    vec3 normal = normal_color * 2.0 - 1.0;
    normal = normalize(mat3(g_vtx_tangent, cross(g_vtx_normal, g_vtx_tangent),
                            g_vtx_normal) * normal);

    vec3 view_ray_ws = normalize(g_vtx_pos - g_shrd_data.cam_pos_and_gamma.xyz);

    float val = g_shrd_data.transp_params_and_time[3] + g_vtx_pos.y * 10.0;
    float kk = 0.75 + 0.25 * step(val - floor(val), 0.5);

    float tr = 0.75 * clamp(1.2 - dot(normal, -view_ray_ws), 0.0, 1.0);

    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(REN_GRID_RES_Z)), 0, REN_GRID_RES_Z - 1);

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    highp uint offset = bitfieldExtract(cell_data.x, 0, 24);
    highp uint pcount = bitfieldExtract(cell_data.y, 8, 8);

    vec3 diffuse_color = texture(SAMPLER2D(g_diff_tex), g_vtx_uvs).xyz;
    vec4 specular_color = texture(SAMPLER2D(g_spec_tex), g_vtx_uvs);

    vec3 refl_ray_ws = reflect(view_ray_ws, normal);

    vec3 reflected_color = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset; i < offset + pcount; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, g_vtx_pos);
        float fade = 1.0 - smoothstep(0.9, 1.0,
                                      dist / g_shrd_data.probes[pi].pos_and_radius.w);

        reflected_color += fade * RGBMDecode(textureLod(g_env_tex,
            vec4(refl_ray_ws, g_shrd_data.probes[pi].unused_and_layer.w), 0.0));
        total_fade += fade;
    }

    if (total_fade > 1.0) {
        reflected_color /= total_fade;
    }

    float alpha = tr * kk;
    g_out_color = vec4(diffuse_color, alpha);
}
