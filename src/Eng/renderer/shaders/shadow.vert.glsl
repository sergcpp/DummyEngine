#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "texturing_common.glsl"

#include "shadow_interface.h"

#pragma multi_compile _ ALPHATEST
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif
#if defined(NO_BINDLESS) && !defined(ALPHATEST)
    #pragma dont_compile
#endif

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef ALPHATEST
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#endif

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    mat4 g_shadow_view_proj_mat;
};
#else
layout(location = U_M_MATRIX_LOC) uniform mat4 g_shadow_view_proj_mat;
#endif

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#ifdef ALPHATEST
    layout(location = 0) out vec2 g_vtx_uvs0;
    layout(location = 1) out flat float g_alpha;
    #if !defined(NO_BINDLESS)
        layout(location = 2) out flat TEX_HANDLE g_alpha_tex;
    #endif // !NO_BINDLESS
#endif // ALPHATEST

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];
    mat4 MMatrix = FetchModelMatrix(g_instances_buf, instance.x);

#ifdef ALPHATEST
    g_vtx_uvs0 = g_in_vtx_uvs0;

    MaterialData mat = g_materials[instance.y];
    g_alpha = 1.0 - mat.params[3].x;
#if !defined(NO_BINDLESS)
    g_alpha_tex = GET_HANDLE(mat.texture_indices[MAT_TEX_ALPHA]);
#endif // !NO_BINDLESS
#endif // ALPHATEST

    vec3 vertex_position_ws = (MMatrix * vec4(g_in_vtx_pos, 1.0)).xyz;
    gl_Position = g_shadow_view_proj_mat * vec4(vertex_position_ws, 1.0);
}
