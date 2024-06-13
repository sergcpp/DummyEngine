#version 430 core
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_fs_common.glsl"
#include "_texturing.glsl"

#pragma multi_compile _ TRANSPARENT

#ifdef TRANSPARENT
#if !defined(BINDLESS_TEXTURES)
    layout(binding = BIND_MAT_TEX3) uniform sampler2D g_alpha_tex;
#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT

#ifdef TRANSPARENT
    layout(location = 0) in vec2 g_vtx_uvs0;
    #if defined(BINDLESS_TEXTURES)
        layout(location = 1) in flat TEX_HANDLE g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT

void main() {
#ifdef TRANSPARENT
    float alpha = texture(SAMPLER2D(g_alpha_tex), g_vtx_uvs0).r;
    if (alpha < 0.5) discard;
#endif
}
