R"(
#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = )" AS_STR(REN_DIFF_TEX_SLOT) R"() uniform sampler2D diffuse_texture;

in vec2 aVertexUVs1_;

void main() {
    float alpha = texture(diffuse_texture, aVertexUVs1_).a;
    if (alpha < 0.5) discard;
}
)"