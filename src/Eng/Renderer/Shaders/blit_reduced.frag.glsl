R"(#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = )" AS_STR(REN_BASE0_TEX_SLOT) R"() uniform sampler2D s_texture;
layout(location = 4) uniform vec2 uOffset;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texture(s_texture, aVertexUVs_ + uOffset).xyz;
    outColor.r = 4.0 * (0.299 * c0.r + 0.587 * c0.g + 0.114 * c0.b);
}
)"