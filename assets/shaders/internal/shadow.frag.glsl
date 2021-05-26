#version 310 es
#extension GL_ARB_bindless_texture: enable

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

#ifdef TRANSPARENT_PERM
#if !defined(GL_ARB_bindless_texture)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D alpha_texture;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs1_;
#else
in vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
in flat uvec2 alpha_texture;
#endif // GL_ARB_bindless_texture
#endif
#endif

void main() {
#ifdef TRANSPARENT_PERM
    float alpha = texture(SAMPLER2D(alpha_texture), aVertexUVs1_).a;
    if (alpha < 0.5) discard;
#endif
}
