R"(
#version 300 es
#ifdef GL_ES
    precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    uOffset : 4
*/
        
uniform sampler2D s_texture;
uniform vec2 uOffset;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texture(s_texture, aVertexUVs_ + uOffset).xyz;
    outColor.r = 0.299 * c0.r + 0.587 * c0.g + 0.114 * c0.b;
}
)"