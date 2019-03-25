R"(
#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = 0) uniform sampler2D s_texture;
layout(binding = 1) uniform mediump sampler2DMS s_mul_texture;
layout(location = 13) uniform vec2 uTexSize;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec2 norm_uvs = aVertexUVs_ / uTexSize;

    vec3 c0 = texture(s_texture, norm_uvs).xyz;
    vec3 c1 = texelFetch(s_mul_texture, ivec2(aVertexUVs_), 0).xyz;
            
    c0 *= c1;

    outColor = vec4(c0, 1.0);
}
)"