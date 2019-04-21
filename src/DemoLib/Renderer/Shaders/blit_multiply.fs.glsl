R"(
#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif
        
)" __ADDITIONAL_DEFINES_STR__ R"(

layout(binding = 0) uniform sampler2D s_texture;
#if defined(MSAA_4)
layout(binding = 1) uniform mediump sampler2DMS s_mul_texture;
#else
layout(binding = 1) uniform mediump sampler2D s_mul_texture;
#endif
layout(location = 13) uniform vec2 uTexSize;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec2 norm_uvs = aVertexUVs_ / uTexSize;

    vec3 c0 = texture(s_texture, norm_uvs).xyz;
#if defined(MSAA_4)
    vec3 c1 = 0.25 * (texelFetch(s_mul_texture, ivec2(aVertexUVs_), 0).xyz +
                      texelFetch(s_mul_texture, ivec2(aVertexUVs_), 1).xyz +
                      texelFetch(s_mul_texture, ivec2(aVertexUVs_), 2).xyz +
                      texelFetch(s_mul_texture, ivec2(aVertexUVs_), 3).xyz);
#else
    vec3 c1 = texelFetch(s_mul_texture, ivec2(aVertexUVs_), 0).xyz;
#endif
            
    c0 *= c1;

    outColor = vec4(c0, 1.0);
}
)"