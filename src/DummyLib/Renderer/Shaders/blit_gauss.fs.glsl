R"(#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

   
layout(binding = )" AS_STR(REN_BASE_TEX_SLOT) R"() uniform sampler2D s_texture;
layout(location = 4) uniform float vertical;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    if(vertical < 0.5) {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(4, 0), 0) * 0.0162162162;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(3, 0), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(2, 0), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(1, 0), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.2270270270;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(1, 0), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(2, 0), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(3, 0), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(4, 0), 0) * 0.0162162162;
    } else {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 4), 0) * 0.0162162162;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 3), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 2), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 1), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.2270270270;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 1), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 2), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 3), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 4), 0) * 0.0162162162;
    }
}
)"