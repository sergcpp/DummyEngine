#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_fs_common.glsl"
#include "_texturing.glsl"

#pragma multi_compile _ TRANSPARENT
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif
#if defined(NO_BINDLESS) && !defined(TRANSPARENT)
    #pragma dont_compile
#endif

#ifdef TRANSPARENT
#if defined(NO_BINDLESS)
    layout(binding = BIND_MAT_TEX3) uniform sampler2D g_alpha_tex;
#endif // NO_BINDLESS
#endif // TRANSPARENT

#ifdef TRANSPARENT
    layout(location = 0) in vec2 g_vtx_uvs0;
    #if !defined(NO_BINDLESS)
        layout(location = 1) in flat TEX_HANDLE g_alpha_tex;
    #endif // !NO_BINDLESS
#endif // TRANSPARENT

void main() {
#ifdef TRANSPARENT
    float alpha = texture(SAMPLER2D(g_alpha_tex), g_vtx_uvs0).r;
    if (alpha < 0.5) discard;
#endif
}
