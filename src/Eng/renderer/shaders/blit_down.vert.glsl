#version 430 core

#include "_vs_common.glsl"
#include "blit_down_interface.h"

layout(location = VTX_POS_LOC) in vec2 g_in_vtx_pos;
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(location = 0) out vec2 g_vtx_uvs;

void main() {
    g_vtx_uvs = g_params.transform.xy + g_in_vtx_uvs * g_params.transform.zw;
    gl_Position = vec4(g_in_vtx_pos, 0.5, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
