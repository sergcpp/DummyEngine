#version 310 es

#include "_vs_common.glsl"
#include "blit_static_vel_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

layout(location = REN_VTX_POS_LOC) in vec2 aVertexPosition;
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) out vec2 aVertexUVs_;

void main() {
    aVertexUVs_ = g_params.transform.xy + aVertexUVs * g_params.transform.zw;
    gl_Position = vec4(aVertexPosition, 0.5, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
