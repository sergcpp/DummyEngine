#version 310 es
#extension GL_EXT_texture_buffer : enable

#include "_vs_common.glsl"
#include "_texturing.glsl"

#include "shadow_interface.glsl"

/*
PERM @TRANSPARENT_PERM
*/

layout(location = REN_VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#endif

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer g_instances_buffer;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    mat4 g_shadow_view_proj_mat;
    ivec2 g_instance_indices[REN_MAX_BATCH_SIZE];
};
#else
layout(location = REN_U_INSTANCES_LOC) uniform ivec2 g_instance_indices[REN_MAX_BATCH_SIZE];
layout(location = U_M_MATRIX_LOC) uniform mat4 g_shadow_view_proj_mat;
#endif

layout(binding = REN_MATERIALS_SLOT, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#ifdef TRANSPARENT_PERM
    LAYOUT(location = 0) out vec2 g_vtx_uvs0;
    #if defined(BINDLESS_TEXTURES)
        LAYOUT(location = 1) out flat TEX_HANDLE g_alpha_texture;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];
    mat4 MMatrix = FetchModelMatrix(g_instances_buffer, instance.x);

#ifdef TRANSPARENT_PERM
    g_vtx_uvs0 = g_in_vtx_uvs0;

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    g_alpha_texture = GET_HANDLE(mat.texture_indices[0]);
#endif // BINDLESS_TEXTURES
#endif

    vec3 vertex_position_ws = (MMatrix * vec4(g_in_vtx_pos, 1.0)).xyz;
    gl_Position = g_shadow_view_proj_mat * vec4(vertex_position_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
