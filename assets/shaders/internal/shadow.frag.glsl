#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

#ifdef TRANSPARENT_PERM
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D alphatest_texture;

in vec2 aVertexUVs1_;
#endif

void main() {
#ifdef TRANSPARENT_PERM
    float alpha = texture(alphatest_texture, aVertexUVs1_).a;
    if (alpha < 0.5) discard;
#endif
}
