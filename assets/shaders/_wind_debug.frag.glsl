#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D g_diff_tex;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D g_norm_tex;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D g_spec_tex;
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D g_decals_tex;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = REN_LIGHT_BUF_SLOT) uniform highp samplerBuffer g_lights_buf;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buf;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buf;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buf;
layout(binding = REN_INST_BUF_SLOT) uniform sampler2D g_noise_tex;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
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
#else
in highp vec3 g_vtx_pos;
in mediump vec2 g_vtx_uvs;
in mediump vec3 g_vtx_normal;
in mediump vec3 g_vtx_tangent;
in highp vec3 g_vtx_sh_uvs[4];
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 g_out_color;
layout(location = REN_OUT_NORM_INDEX) out vec4 g_out_normal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 g_out_specular;

void main(void) {
    vec3 lo_freq_bend_dir = 0.5 * texture(g_noise_tex, g_shrd_data.wind_scroll.xy +
                                          (1.0 / 256.0) * vec2(g_vtx_pos.xz)).rgb + vec3(0.5);
    vec3 hi_freq_bend_dir = 0.5 * texture(g_noise_tex, g_shrd_data.wind_scroll.zw +
                                          (1.0 / 8.0) * vec2(g_vtx_pos.xz)).rgb + vec3(0.5);

    g_out_color = vec4(pow(lo_freq_bend_dir + hi_freq_bend_dir, vec3(2.2)), 1.0);
    g_out_normal = vec4(0.0);
    g_out_specular = vec4(0.0);
}
