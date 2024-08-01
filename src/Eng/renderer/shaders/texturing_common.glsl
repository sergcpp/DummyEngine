#ifndef _TEXTURING_GLSL
#define _TEXTURING_GLSL

#if defined(VULKAN) || defined(GL_ARB_bindless_texture)
#define BINDLESS_TEXTURES
#endif

#if defined(VULKAN)
#extension GL_EXT_nonuniform_qualifier : enable
    #define TEX_HANDLE uint
    #define SAMPLER2D(ndx) scene_textures[nonuniformEXT(ndx)]
    #define GET_HANDLE

    #ifndef NO_BINDING_DECLARATION
        layout(set = BIND_SET_SCENETEXTURES, binding = BIND_BINDLESS_TEX) uniform sampler2D scene_textures[];
    #endif
#else // VULKAN
    #if defined(GL_ARB_bindless_texture)
        #define TEX_HANDLE uvec2
        #define SAMPLER2D(ndx) sampler2D(ndx)
        #define GET_HANDLE(ndx) texture_handles[ndx]

        #ifndef NO_BINDING_DECLARATION
            layout(binding = BIND_BINDLESS_TEX, std430) readonly buffer TextureHandles {
                uvec2 texture_handles[];
            };
        #endif
    #else // GL_ARB_bindless_texture
        #define SAMPLER2D
    #endif // GL_ARB_bindless_texture
#endif // VULKAN

#endif // _TEXTURING_GLSL