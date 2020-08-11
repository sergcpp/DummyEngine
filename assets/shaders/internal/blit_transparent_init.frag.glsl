#version 310 es
#extension GL_ARB_texture_multisample : enable
#ifdef GL_ES
    precision mediump float;
#endif

in vec2 aVertexUVs_;

layout(location = 0) out vec4 outColorAccum;
layout(location = 1) out vec4 outAlphaAndRevealage;

void main() {
    outColorAccum = vec4(0.0);
    outAlphaAndRevealage.r = 1.0;
}

