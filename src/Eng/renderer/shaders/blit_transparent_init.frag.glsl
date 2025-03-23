#version 430 core
#extension GL_ARB_texture_multisample : enable

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color_accum;
layout(location = 1) out vec4 g_out_alpha_and_revealage;

void main() {
    g_out_color_accum = vec4(0.0);
    g_out_alpha_and_revealage.x = 1.0;
}

