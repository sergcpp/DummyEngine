R"(
#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = )" AS_STR(REN_DIFF_TEX_SLOT) R"() uniform sampler2D s_texture;
uniform float near;
uniform float far;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    float depth = texelFetch(s_texture, ivec2(aVertexUVs_), 0).r;
    if (near > 0.0001) {
        // cam is not orthographic
        depth = (near * far) / (depth * (near - far) + far);
        depth /= far;
    }
    outColor = vec4(depth, depth, depth, 1.0);
}
)"