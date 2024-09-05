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
#if defined(NO_BINDLESS) && !defined(ALPHATEST)
    #pragma dont_compile
#endif

#ifdef ALPHATEST
#if defined(NO_BINDLESS)
    layout(binding = BIND_MAT_TEX3) uniform sampler2D g_alpha_tex;
#endif // NO_BINDLESS
#endif // ALPHATEST

#ifdef ALPHATEST
    layout(location = 0) in vec2 g_vtx_uvs0;
    layout(location = 1) in flat float g_alpha;
    #if !defined(NO_BINDLESS)
        layout(location = 2) in flat TEX_HANDLE g_alpha_tex;
    #endif // !NO_BINDLESS
#endif // ALPHATEST

void main() {
#ifdef ALPHATEST
    float alpha = g_alpha * texture(SAMPLER2D(g_alpha_tex), g_vtx_uvs0).r;
    if (alpha < 0.5) {
        discard;
    }
#endif
}
