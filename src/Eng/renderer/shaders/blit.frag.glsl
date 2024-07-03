#version 430 core

#include "_fs_common.glsl"

layout(binding = BIND_BASE0_TEX) uniform sampler2D g_tex;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) float g_multiplier;
};
#else
layout(location = 4) uniform float g_multiplier;
#endif

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

void main() {
    g_out_color = vec4(g_multiplier, g_multiplier, g_multiplier, 1.0) * texelFetch(g_tex, ivec2(g_vtx_uvs), 0);
}
