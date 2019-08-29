R"(#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = )" AS_STR(REN_BASE_TEX_SLOT) R"() uniform sampler2D s_texture;
layout(location = 1) uniform float near;
layout(location = 2) uniform float far;
layout(location = 3) uniform vec3 color;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    float depth = texelFetch(s_texture, ivec2(aVertexUVs_), 0).r;
    if (near > 0.0001) {
        // cam is not orthographic
        depth = (near * far) / (depth * (near - far) + far);
        depth /= far;
    }
    outColor = vec4(vec3(depth) * color, 1.0);
}
)"