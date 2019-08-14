R"(#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = )" AS_STR(REN_BASE_TEX_SLOT) R"() uniform lowp sampler2D source_texture;

in vec2 aVertexUVs_;

out vec3 outColor;

void main() {
    ivec2 icoord = ivec2(aVertexUVs_);

    outColor = texelFetch(source_texture, icoord, 0).rgb;

    if (outColor.b < 0.1) {
        vec3 color;
        float normalization = 0.0;

        color = texelFetch(source_texture, icoord - ivec2(1, 0), 0).rgb;
        outColor.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(source_texture, icoord + ivec2(1, 0), 0).rgb;
        outColor.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(source_texture, icoord - ivec2(0, 1), 0).rgb;
        outColor.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(source_texture, icoord + ivec2(0, 1), 0).rgb;
        outColor.rg += color.rg * color.b;
        normalization += color.b;

        outColor.rg /= normalization;
    }
}
)"