#version 430

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#line 0
#ifndef SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_H
#define SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_H

#line 0

#ifndef INTERFACE_COMMON_GLSL
#define INTERFACE_COMMON_GLSL

#ifdef __cplusplus
#define VEC2_TYPE Ren::Vec2f
#define VEC3_TYPE Ren::Vec3f
#define VEC4_TYPE Ren::Vec4f

#define IVEC2_TYPE Ren::Vec2i
#define IVEC3_TYPE Ren::Vec3i
#define IVEC4_TYPE Ren::Vec4i

#define UINT_TYPE uint32_t
#define UVEC2_TYPE Ren::Vec2u
#define UVEC3_TYPE Ren::Vec3u
#define UVEC4_TYPE Ren::Vec4u

#define MAT2_TYPE Ren::Mat2f
#define MAT3_TYPE Ren::Mat3f
#define MAT4_TYPE Ren::Mat4f

#define INTERFACE_START(name) namespace name {
#define INTERFACE_END }

#define DEF_CONST_INT(name, index) const int name = index;
#else // __cplusplus
#define VEC2_TYPE vec2
#define VEC3_TYPE vec3
#define VEC4_TYPE vec4

#define IVEC2_TYPE ivec2
#define IVEC3_TYPE ivec3
#define IVEC4_TYPE ivec4

#define UINT_TYPE uint
#define UVEC2_TYPE uvec2
#define UVEC3_TYPE uvec3
#define UVEC4_TYPE uvec4

#define MAT2_TYPE mat2
#define MAT3_TYPE mat3
#define MAT4_TYPE mat4

#define INTERFACE_START(name)
#define INTERFACE_END

#if defined(VULKAN)
#define LAYOUT_PARAMS layout(push_constant)
#elif defined(GL_SPIRV)
#define LAYOUT_PARAMS layout(binding = REN_UB_UNIF_PARAM_LOC, std140)
#else
#define LAYOUT_PARAMS layout(std140)
#endif
#endif // __cplusplus

#endif // INTERFACE_COMMON_GLSL

#line 0

INTERFACE_START(SSRWriteIndirRTDispatch)

#define RAY_COUNTER_SLOT 0
#define INDIR_ARGS_SLOT 1

INTERFACE_END

#endif // SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_H

#line 9

layout(std430, binding = RAY_COUNTER_SLOT) buffer RayCounter {
    uint g_ray_counter[];
};
layout(std430, binding = INDIR_ARGS_SLOT) writeonly buffer IndirArgs {
    uint g_intersect_args[];
};

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint ray_count = g_ray_counter[4];

    // raytracing pipeline dispatch
    g_intersect_args[0] = ray_count;
    g_intersect_args[1] = 1;
    g_intersect_args[2] = 1;

    // compute pipeline dispatch (for inlined raytracing)
    g_intersect_args[3] = (ray_count + 63) / 64;
    g_intersect_args[4] = 1;
    g_intersect_args[5] = 1;

    g_ray_counter[4] = 0;
    g_ray_counter[5] = ray_count;
}
