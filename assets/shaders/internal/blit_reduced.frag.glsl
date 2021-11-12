#version 310 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_reduced_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

layout(binding = SRC_TEX_SLOT) uniform sampler2D s_texture;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in vec2 aVertexUVs_;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 c0 = texture(s_texture, aVertexUVs_ + g_params.offset.xy).xyz;
    outColor.r = dot(c0, vec3(0.299, 0.587, 0.114));
}
