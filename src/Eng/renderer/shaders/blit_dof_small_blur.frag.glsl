#version 310 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D g_tex;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) highp vec4 g_transform;
};
#else
layout(location = 0) uniform highp vec4 g_transform;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    vec2 norm_uvs = g_vtx_uvs / g_transform.zw;
    vec2 px_offset = 0.5 / g_transform.zw;

    vec4 color = vec4(0.0);
    color += textureLod(g_tex, norm_uvs - px_offset, 0.0);
    color += textureLod(g_tex, norm_uvs + vec2(px_offset.x, -px_offset.y), 0.0);
    color += textureLod(g_tex, norm_uvs + px_offset, 0.0);
    color += textureLod(g_tex, norm_uvs + vec2(-px_offset.x, px_offset.y), 0.0);
    color /= 4.0;

    g_out_color = color;
}
