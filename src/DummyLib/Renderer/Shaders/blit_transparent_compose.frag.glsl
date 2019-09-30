R"(#version 310 es
#extension GL_ARB_texture_multisample : enable
#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = )" AS_STR(REN_BASE0_TEX_SLOT) R"() uniform mediump sampler2D s_transparent_texture;
layout(binding = )" AS_STR(REN_BASE1_TEX_SLOT) R"() uniform mediump sampler2D s_moments0_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    ivec2 icoord = ivec2(aVertexUVs_);

    vec4 transparent_color = texelFetch(s_transparent_texture, icoord, 0);
    float b0 = texelFetch(s_moments0_texture, icoord, 0).x;

    if (transparent_color.w > 0.0) {
        transparent_color.xyz /= transparent_color.w;
    }

    float k = exp(-b0);
    outColor = vec4(transparent_color.xyz, 1.0 - k);
}
)"
