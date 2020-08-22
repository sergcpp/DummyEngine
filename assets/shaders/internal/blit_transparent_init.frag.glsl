#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
    precision mediump float;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColorAccum;
layout(location = 1) out vec4 outAlphaAndRevealage;

void main() {
    outColorAccum = vec4(0.0);
    outAlphaAndRevealage.r = 1.0;
}

