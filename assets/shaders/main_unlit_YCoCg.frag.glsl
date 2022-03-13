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
#include "internal/_texturing.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D g_mat0_texture;
#endif // BINDLESS_TEXTURES

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT(location = 4) in vec2 g_vtx_uvs0;
#if defined(BINDLESS_TEXTURES)
LAYOUT(location = 8) in flat TEX_HANDLE g_mat0_texture;
#endif // BINDLESS_TEXTURES

layout(location = REN_OUT_COLOR_INDEX) out vec4 g_out_color;
layout(location = REN_OUT_NORM_INDEX) out vec4 g_out_normal;

void main(void) {
    vec4 col = texture(SAMPLER2D(g_mat0_texture), g_vtx_uvs0);

    float scale = (col.b * (255.0 / 8.0)) + 1.0;
    float Y = col.a;
    float Co = (col.r - (0.5 * 256.0 / 255.0)) / scale;
    float Cg = (col.g - (0.5 * 256.0 / 255.0)) / scale;

    vec3 col_rgb;
    col_rgb.r = Y + Co - Cg;
    col_rgb.g = Y + Cg;
    col_rgb.b = Y - Co - Cg;

    g_out_color = vec4(SRGBToLinear(col_rgb), 1.0);
    g_out_normal = vec4(0.0);
}
