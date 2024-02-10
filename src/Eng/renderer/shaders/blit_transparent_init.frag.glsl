#version 320 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
    precision mediump float;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color_accum;
layout(location = 1) out vec4 g_out_alpha_and_revealage;

void main() {
    g_out_color_accum = vec4(0.0);
    g_out_alpha_and_revealage.r = 1.0;
}

