#version 430 core
#extension GL_ARB_shading_language_packing : require

#include "_cs_common.glsl"
#include "clear_image_interface.h"

#pragma multi_compile _ ARRAY _3D
#pragma multi_compile RGBA8 R32F R16F R8 R32UI RG8 RG16F RG32F RGBA32F RGBA16F RG11F_B10F

#if defined(ARRAY)
    #if defined(R32UI)
        #define IMAGE_TYPE uimage2DArray
    #else
        #define IMAGE_TYPE image2DArray
    #endif
#elif defined(_3D)
    #if defined(R32UI)
        #define IMAGE_TYPE uimage3D
    #else
        #define IMAGE_TYPE image3D
    #endif
#else
    #if defined(R32UI)
        #define IMAGE_TYPE uimage2D
    #else
        #define IMAGE_TYPE image2D
    #endif
#endif

#if defined(R32UI)
    #define ZERO uvec4(0)
#else
    #define ZERO vec4(0)
#endif

layout(binding = OUT_IMG_SLOT,
#if defined(RGBA8)
    rgba8
#elif defined(R32F)
    r32f
#elif defined(R16F)
    r16f
#elif defined(R8)
    r8
#elif defined(R32UI)
    r32ui
#elif defined(RG8)
    rg8
#elif defined(RG16F)
    rg16f
#elif defined(RG32F)
    rg32f
#elif defined(RG11F_B10F)
    r11f_g11f_b10f
#elif defined(RGBA32F)
    rgba32f
#elif defined(RGBA16F)
    rgba16f
#endif
) uniform writeonly IMAGE_TYPE g_out_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
#if defined(ARRAY) || defined(_3D)
    imageStore(g_out_img, ivec3(gl_GlobalInvocationID.xyz), ZERO);
#else
    imageStore(g_out_img, ivec2(gl_GlobalInvocationID.xy), ZERO);
#endif
}
