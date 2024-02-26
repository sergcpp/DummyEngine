#version 320 es
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "_texturing.glsl"

#pragma multi_compile _ TRANSPARENT_PERM

#ifdef TRANSPARENT_PERM
#if !defined(BINDLESS_TEXTURES)
    layout(binding = BIND_MAT_TEX3) uniform sampler2D g_alpha_tex;
#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

#ifdef TRANSPARENT_PERM
    LAYOUT(location = 0) in highp vec2 g_vtx_uvs0;
    #if defined(BINDLESS_TEXTURES)
        LAYOUT(location = 1) in flat TEX_HANDLE g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

void main() {
#ifdef TRANSPARENT_PERM
    float alpha = texture(SAMPLER2D(g_alpha_tex), g_vtx_uvs0).r;
    if (alpha < 0.5) discard;
#endif
}
