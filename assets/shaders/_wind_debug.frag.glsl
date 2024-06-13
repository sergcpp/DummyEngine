#version 430 core
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

#include "internal/_fs_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

layout(binding = BIND_MAT_TEX0) uniform sampler2D g_diff_tex;
layout(binding = BIND_MAT_TEX1) uniform sampler2D g_norm_tex;
layout(binding = BIND_MAT_TEX2) uniform sampler2D g_spec_tex;
layout(binding = BIND_SHAD_TEX) uniform sampler2DShadow g_shadow_tex;
layout(binding = BIND_DECAL_TEX) uniform sampler2D g_decals_tex;
layout(binding = BIND_SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = BIND_LIGHT_BUF) uniform samplerBuffer g_lights_buf;
layout(binding = BIND_DECAL_BUF) uniform samplerBuffer g_decals_buf;
layout(binding = BIND_CELLS_BUF) uniform usamplerBuffer g_cells_buf;
layout(binding = BIND_ITEMS_BUF) uniform usamplerBuffer g_items_buf;
layout(binding = BIND_INST_BUF) uniform sampler2D g_noise_tex;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#if defined(VULKAN)
layout(location = 0) in vec3 g_vtx_pos;
layout(location = 1) in vec2 g_vtx_uvs;
layout(location = 2) in vec3 g_vtx_normal;
layout(location = 3) in vec3 g_vtx_tangent;
layout(location = 4) in vec3 g_vtx_sh_uvs[4];
#else
in vec3 g_vtx_pos;
in vec2 g_vtx_uvs;
in vec3 g_vtx_normal;
in vec3 g_vtx_tangent;
in vec3 g_vtx_sh_uvs[4];
#endif

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;
layout(location = LOC_OUT_NORM) out vec4 g_out_normal;
layout(location = LOC_OUT_SPEC) out vec4 g_out_specular;

void main(void) {
    vec3 lo_freq_bend_dir = 0.5 * texture(g_noise_tex, g_shrd_data.wind_scroll.xy +
                                          (1.0 / 256.0) * vec2(g_vtx_pos.xz)).rgb + vec3(0.5);
    vec3 hi_freq_bend_dir = 0.5 * texture(g_noise_tex, g_shrd_data.wind_scroll.zw +
                                          (1.0 / 8.0) * vec2(g_vtx_pos.xz)).rgb + vec3(0.5);

    g_out_color = vec4(pow(lo_freq_bend_dir + hi_freq_bend_dir, vec3(2.2)), 1.0);
    g_out_normal = vec4(0.0);
    g_out_specular = vec4(0.0);
}
