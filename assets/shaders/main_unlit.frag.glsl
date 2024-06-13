#version 430 core
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
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
layout(binding = BIND_ENV_TEX) uniform samplerCubeArray g_env_tex;
layout(binding = BIND_LIGHT_BUF) uniform samplerBuffer g_lights_buf;
layout(binding = BIND_DECAL_BUF) uniform samplerBuffer g_decals_buf;
layout(binding = BIND_CELLS_BUF) uniform usamplerBuffer g_cells_buf;
layout(binding = BIND_ITEMS_BUF) uniform usamplerBuffer g_items_buf;
layout(binding = BIND_CONE_RT_LUT) uniform sampler2D g_cone_rt_lut;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(location = 1) in vec2 g_vtx_uvs;
layout(location = 2) in vec3 g_vtx_normal;
#if defined(BINDLESS_TEXTURES)
    layout(location = 8) in flat TEX_HANDLE g_diff_tex;
#endif // BINDLESS_TEXTURES

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;
layout(location = LOC_OUT_NORM) out vec4 g_out_normal;
layout(location = LOC_OUT_SPEC) out vec4 g_out_specular;

void main(void) {
    vec3 albedo_color = SRGBToLinear(YCoCg_to_RGB(texture(SAMPLER2D(g_diff_tex), g_vtx_uvs)));

    g_out_color = vec4(albedo_color, 1.0);
    g_out_normal = PackNormalAndRoughness(g_vtx_normal, 0.0);
}
