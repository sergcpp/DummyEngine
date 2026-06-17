#version 430 core

#include "_fs_common.glsl"
#include "blit_oit_depth_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = OIT_DEPTH_BUF_SLOT) uniform usamplerBuffer g_oit_depth_buf;

layout(location = 0) in vec2 g_vtx_uvs;

void main() {
    uint frag_index = g_params.layer_index * g_params.img_size.x * g_params.img_size.y;
    frag_index += uint(gl_FragCoord.y) * g_params.img_size.x + uint(gl_FragCoord.x);

    gl_FragDepth = uintBitsToFloat(texelFetch(g_oit_depth_buf, int(frag_index)).x);
}
