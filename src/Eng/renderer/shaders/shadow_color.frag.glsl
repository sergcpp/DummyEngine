#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_fs_common.glsl"
#include "texturing_common.glsl"

#pragma multi_compile _ ALPHATEST
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

#if defined(NO_BINDLESS)
    layout(binding = BIND_MAT_TEX0) uniform sampler2D g_base_tex;
    layout(binding = BIND_MAT_TEX3) uniform sampler2D g_alpha_tex;
#endif // NO_BINDLESS

layout(location = 0) in vec2 g_vtx_uvs;
#if !defined(NO_BINDLESS)
    layout(location = 1) in flat TEX_HANDLE g_base_tex;
#endif
layout(location = 2) in flat vec4 g_base_color;
#if !defined(NO_BINDLESS)
    layout(location = 3) in flat TEX_HANDLE g_alpha_tex;
#endif // !NO_BINDLESS
layout(location = 4) in flat float g_alpha;

layout(location = 0) out vec4 g_out_color;

void main() {
    const float alpha = g_alpha * textureBindless(g_alpha_tex, g_vtx_uvs).x;
#ifdef ALPHATEST
    if (alpha < 0.5) {
        discard;
    }
#endif
    const vec3 base_color = g_base_color.xyz * YCoCg_to_RGB(textureBindless(g_base_tex, g_vtx_uvs));
    g_out_color = vec4(mix(vec3(1.0), 0.8 * g_base_color.w * base_color, alpha), gl_FragCoord.z);
}
