#version 430 core
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "_texturing.glsl"

#include "shadow_interface.h"

#pragma multi_compile _ TRANSPARENT

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef TRANSPARENT
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

#ifdef TRANSPARENT
    layout(location = 0) out vec2 g_vtx_uvs0;
    #if defined(BINDLESS_TEXTURES)
        layout(location = 1) out flat TEX_HANDLE g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];
    mat4 MMatrix = FetchModelMatrix(g_instances_buf, instance.x);

#ifdef TRANSPARENT
    g_vtx_uvs0 = g_in_vtx_uvs0;

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    g_alpha_tex = GET_HANDLE(mat.texture_indices[4]);
#endif // BINDLESS_TEXTURES
#endif

    vec3 vertex_position_ws = (MMatrix * vec4(g_in_vtx_pos, 1.0)).xyz;
    gl_Position = g_shadow_view_proj_mat * vec4(vertex_position_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
