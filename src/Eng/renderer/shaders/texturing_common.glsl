#ifndef TEXTURING_COMMON_GLSL
#define TEXTURING_COMMON_GLSL

#if defined(VULKAN) || defined(GL_ARB_bindless_texture)
#define BINDLESS_TEXTURES
#endif

#if defined(VULKAN)
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require
    #define TEX_HANDLE uint
    #define TEXTURE2D(ndx) g_scene_textures[nonuniformEXT(ndx)]
    #define GET_HANDLE

    layout(set = BIND_SET_SCENETEXTURES, binding = BIND_BINDLESS_TEX) uniform texture2D g_scene_textures[];
    layout(set = BIND_SET_SCENETEXTURES, binding = BIND_SCENE_SAMPLERS) uniform sampler g_linear_sampler; // Only one sampler for now

    vec4 textureBindless(const uint handle, const vec2 uvs) {
        return texture(sampler2D(TEXTURE2D(handle), g_linear_sampler), uvs);
    }
    vec4 textureLodBindless(const uint handle, const vec2 uvs, const float lod) {
        return textureLod(sampler2D(TEXTURE2D(handle), g_linear_sampler), uvs, lod);
    }
    vec4 textureGradBindless(const uint handle, const vec2 P, const vec2 dPdx, const vec2 dPdy) {
        return textureGrad(sampler2D(TEXTURE2D(handle), g_linear_sampler), P, dPdx, dPdy);
    }
    vec2 textureQueryLodBindless(const uint handle, const vec2 P) {
        return textureQueryLod(sampler2D(TEXTURE2D(handle), g_linear_sampler), P);
    }
#else // VULKAN
    #if defined(GL_ARB_bindless_texture)
        #define TEX_HANDLE uvec2
        #define TEXTURE2D(ndx) sampler2D(ndx)
        #define GET_HANDLE(ndx) g_texture_handles[ndx]

        layout(binding = BIND_BINDLESS_TEX, std430) readonly buffer TextureHandles {
            uvec2 g_texture_handles[];
        };
        layout(binding = BIND_SCENE_SAMPLERS) uniform sampler g_linear_sampler;

        vec4 textureBindless(const uvec2 handle, const vec2 uvs) {
            return texture(sampler2D(handle), uvs);
        }
        vec4 textureLodBindless(const uvec2 handle, const vec2 uvs, const float lod) {
            return textureLod(sampler2D(handle), uvs, lod);
        }
        vec4 textureGradBindless(const uvec2 handle, const vec2 P, const vec2 dPdx, const vec2 dPdy) {
            return textureGrad(sampler2D(handle), P, dPdx, dPdy);
        }
    #else // GL_ARB_bindless_texture
        #define TEX_HANDLE sampler2D
        #define TEXTURE2D

        layout(binding = BIND_SCENE_SAMPLERS) uniform sampler g_linear_sampler;

        vec4 textureBindless(const sampler2D handle, const vec2 uvs) {
            return texture(handle, uvs);
        }
        vec4 textureLodBindless(const sampler2D handle, const vec2 uvs, const float lod) {
            return textureLod(handle, uvs, lod);
        }
        vec4 textureGradBindless(const sampler2D handle, const vec2 P, const vec2 dPdx, const vec2 dPdy) {
            return textureGrad(handle, P, dPdx, dPdy);
        }
    #endif // GL_ARB_bindless_texture
#endif // VULKAN

vec4 texelFetchBindless(const TEX_HANDLE handle, ivec2 P, int lod) {
    return texelFetch(TEXTURE2D(handle), P, lod);
}
ivec2 textureSizeBindless(const TEX_HANDLE handle, const int lod) {
    return textureSize(TEXTURE2D(handle), lod);
}
int textureQueryLevelsBindless(const TEX_HANDLE handle) {
    return textureQueryLevels(TEXTURE2D(handle));
}


#endif // TEXTURING_COMMON_GLSL