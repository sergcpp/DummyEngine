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

layout(binding = SRC_TEX_SLOT) uniform sampler2D g_texture;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

void main() {
    vec3 c0 = texture(g_texture, g_vtx_uvs + g_params.offset.xy).xyz;
    g_out_color.r = dot(c0, vec3(0.299, 0.587, 0.114));
}
