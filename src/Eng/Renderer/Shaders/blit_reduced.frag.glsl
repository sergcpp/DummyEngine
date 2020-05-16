R"(#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif

)"
#include "_fs_common.glsl"
R"(

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;
layout(location = 4) uniform vec2 uOffset;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texture(s_texture, aVertexUVs_ + uOffset).xyz;
    outColor.r = dot(c0, vec3(0.299, 0.587, 0.114));
}
)"