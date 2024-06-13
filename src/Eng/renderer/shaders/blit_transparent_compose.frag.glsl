#version 320 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = BIND_BASE0_TEX) uniform mediump sampler2D s_accum_tex;
layout(binding = BIND_BASE1_TEX) uniform mediump sampler2D s_additional_tex;

#if defined(VULKAN)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    ivec2 icoord = ivec2(g_vtx_uvs);

    vec4 accum = texelFetch(s_accum_tex, icoord, 0);

#if (OIT_MODE == OIT_MOMENT_BASED)
    float b0 = texelFetch(s_additional_tex, icoord, 0).x;

#if OIT_MOMENT_RENORMALIZE
    if (accum.w > 0.0000001) {
        accum.xyz /= accum.w;
    }
#endif

    float k = exp(-b0);
    g_out_color = vec4(accum.xyz, k);
#elif (OIT_MODE == OIT_WEIGHTED_BLENDED)
    float revealage = texelFetch(s_additional_tex, icoord, 0).x;

    g_out_color = vec4(accum.rgb / clamp(accum.a, 1e-4, 5e4), revealage);
#endif
}

