#version 320 es
#extension GL_EXT_texture_cube_map_array : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

#if defined(VULKAN)
layout(location = 0) in vec3 g_vtx_pos;
#else
in vec3 g_vtx_pos;
#endif

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;

void main() {
    g_out_color = vec4(1.0, 0.0, 0.0, 1.0);
}
