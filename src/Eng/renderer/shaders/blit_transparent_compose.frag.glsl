#version 310 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

/*
PERM @MSAA_4
*/

#if defined(MSAA_4)
layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2DMS s_accum_tex;
layout(binding = REN_BASE1_TEX_SLOT) uniform mediump sampler2DMS s_additional_tex;
#else
layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2D s_accum_tex;
layout(binding = REN_BASE1_TEX_SLOT) uniform mediump sampler2D s_additional_tex;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    ivec2 icoord = ivec2(g_vtx_uvs);

#if defined(MSAA_4)
    vec4 accum = 0.25 * (texelFetch(s_accum_tex, icoord, 0) +
                         texelFetch(s_accum_tex, icoord, 1) +
                         texelFetch(s_accum_tex, icoord, 2) +
                         texelFetch(s_accum_tex, icoord, 3));
#else
    vec4 accum = texelFetch(s_accum_tex, icoord, 0);
#endif

#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
    float b0 = texelFetch(s_additional_tex, icoord, 0).x;

#if REN_OIT_MOMENT_RENORMALIZE
    if (accum.w > 0.0000001) {
        accum.xyz /= accum.w;
    }
#endif

    float k = exp(-b0);
    g_out_color = vec4(accum.xyz, k);
#elif (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED)
    float revealage = texelFetch(s_additional_tex, icoord, 0).x;

    g_out_color = vec4(accum.rgb / clamp(accum.a, 1e-4, 5e4), revealage);
#endif
}

