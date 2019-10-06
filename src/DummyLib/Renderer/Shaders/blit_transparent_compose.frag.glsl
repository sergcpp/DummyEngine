R"(#version 310 es
#extension GL_ARB_texture_multisample : enable
#ifdef GL_ES
    precision mediump float;
#endif

)" __ADDITIONAL_DEFINES_STR__ R"(

#define REN_OIT_MOMENT_RENORMALIZE )" AS_STR(REN_OIT_MOMENT_RENORMALIZE) R"(

#if defined(MSAA_4)
layout(binding = )" AS_STR(REN_BASE0_TEX_SLOT) R"() uniform mediump sampler2DMS s_transparent_texture;
layout(binding = )" AS_STR(REN_BASE1_TEX_SLOT) R"() uniform mediump sampler2DMS s_moments0_texture;
#else
layout(binding = )" AS_STR(REN_BASE0_TEX_SLOT) R"() uniform mediump sampler2D s_transparent_texture;
layout(binding = )" AS_STR(REN_BASE1_TEX_SLOT) R"() uniform mediump sampler2D s_moments0_texture;
#endif

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    ivec2 icoord = ivec2(aVertexUVs_);

#if defined(MSAA_4)
    vec4 transparent_color = 0.25 * (texelFetch(s_transparent_texture, icoord, 0) +
                                     texelFetch(s_transparent_texture, icoord, 1) +
                                     texelFetch(s_transparent_texture, icoord, 2) +
                                     texelFetch(s_transparent_texture, icoord, 3));
#else
    vec4 transparent_color = texelFetch(s_transparent_texture, icoord, 0);
#endif

    float b0 = texelFetch(s_moments0_texture, icoord, 0).x;

#if REN_OIT_MOMENT_RENORMALIZE
    if (transparent_color.w > 0.0000001) {
        transparent_color.xyz /= transparent_color.w;
    }
#endif

    float k = exp(-b0);
    outColor = vec4(transparent_color.xyz, k);
}
)"
