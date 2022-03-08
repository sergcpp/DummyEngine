#version 310 es

#include "_vs_common.glsl"
#include "blit_combine_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

layout(location = REN_VTX_POS_LOC) in vec2 g_in_vtx_pos;
layout(location = REN_VTX_UV1_LOC) in vec2 g_in_vtx_uvs;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) out vec2 g_vtx_uvs;

void main() {
    g_vtx_uvs = g_params.transform.xy + g_in_vtx_uvs * g_params.transform.zw;
    gl_Position = vec4(g_in_vtx_pos, 0.5, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
