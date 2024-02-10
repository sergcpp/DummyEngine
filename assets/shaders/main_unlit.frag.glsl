#version 320 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"
#include "internal/_texturing.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

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

layout (binding = REN_UB_SHARED_DATA_LOC, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT(location = 1) in vec2 g_vtx_uvs;
LAYOUT(location = 2) in mediump vec3 g_vtx_normal;
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 8) in flat TEX_HANDLE g_diff_tex;
#endif // BINDLESS_TEXTURES

layout(location = REN_OUT_COLOR_INDEX) out vec4 g_out_color;
layout(location = REN_OUT_NORM_INDEX) out vec4 g_out_normal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 g_out_specular;

void main(void) {
    vec3 albedo_color = SRGBToLinear(YCoCg_to_RGB(texture(SAMPLER2D(g_diff_tex), g_vtx_uvs)));

    g_out_color = vec4(albedo_color, 1.0);
    g_out_normal = PackNormalAndRoughness(g_vtx_normal, 0.0);
}
