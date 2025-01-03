#version 430 core

#include "_vs_common.glsl"

layout(location = VTX_POS_LOC) in vec2 g_in_vtx_pos;
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs;

void main() {
    gl_Position = vec4(g_in_vtx_pos, 0.0, 1.0);
}
