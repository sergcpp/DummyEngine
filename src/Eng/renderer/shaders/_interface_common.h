
#ifndef INTERFACE_COMMON_GLSL
#define INTERFACE_COMMON_GLSL

#ifdef __cplusplus
#define vec2 Ren::Vec2f
#define vec3 Ren::Vec3f
#define vec4 Ren::Vec4f

#define ivec2 Ren::Vec2i
#define ivec3 Ren::Vec3i
#define ivec4 Ren::Vec4i

#define uint uint32_t
#define uvec2 Ren::Vec2u
#define uvec3 Ren::Vec3u
#define uvec4 Ren::Vec4u

#define mat2 Ren::Mat2f
#define mat3 Ren::Mat3f
#define mat4 Ren::Mat4f

#define INTERFACE_START(name) namespace name {
#define INTERFACE_END }
#else // __cplusplus
#define INTERFACE_START(name)
#define INTERFACE_END

#if defined(VULKAN)
#define LAYOUT_PARAMS layout(push_constant)
#else
#define LAYOUT_PARAMS layout(binding = BIND_PUSH_CONSTANT_BUF, std140)
#endif
#endif // __cplusplus

#endif // INTERFACE_COMMON_GLSL
