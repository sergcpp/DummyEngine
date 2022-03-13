#version 310 es
#extension GL_EXT_texture_buffer : enable

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "_texturing.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
PERM @MOVING_PERM
PERM @TRANSPARENT_PERM
PERM @MOVING_PERM;TRANSPARENT_PERM
*/

layout(location = REN_VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer g_instances_buffer;

layout(binding = REN_INST_INDICES_BUF_SLOT, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = REN_MATERIALS_SLOT, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#ifdef MOVING_PERM
    LAYOUT(location = 0) out vec3 g_vtx_pos_cs_curr;
    LAYOUT(location = 1) out vec3 g_vtx_pos_cs_prev;
#endif // MOVING_PERM
#ifdef TRANSPARENT_PERM
    LAYOUT(location = 2) out vec2 g_vtx_uvs0;
    #ifdef HASHED_TRANSPARENCY
        LAYOUT(location = 3) out vec3 g_vtx_pos_ls;
    #endif // HASHED_TRANSPARENCY
    #if defined(BINDLESS_TEXTURES)
        LAYOUT(location = 4) out flat TEX_HANDLE g_alpha_texture;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

invariant gl_Position;

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];

    mat4 model_matrix_curr = FetchModelMatrix(g_instances_buffer, instance.x);
#ifdef MOVING_PERM
    mat4 model_matrix_prev = FetchModelMatrix(g_instances_buffer, instance.x + 1);
#endif // MOVING_PERM

#ifdef TRANSPARENT_PERM
    g_vtx_uvs0 = g_in_vtx_uvs0;

    MaterialData mat = g_materials[instance.y];
#if defined(BINDLESS_TEXTURES)
    g_alpha_texture = GET_HANDLE(mat.texture_indices[0]);
#endif // BINDLESS_TEXTURES
#ifdef HASHED_TRANSPARENCY
    g_vtx_pos_ls = g_in_vtx_pos;
#endif // HASHED_TRANSPARENCY
#endif // TRANSPARENT_PERM

    vec3 vtx_pos_ws_curr = (model_matrix_curr * vec4(g_in_vtx_pos, 1.0)).xyz;
    gl_Position = g_shrd_data.view_proj_matrix * vec4(vtx_pos_ws_curr, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif

#ifdef MOVING_PERM
    g_vtx_pos_cs_curr = gl_Position.xyw;

    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(g_in_vtx_pos, 1.0)).xyz;
    g_vtx_pos_cs_prev = (g_shrd_data.view_proj_prev_matrix * vec4(vtx_pos_ws_prev, 1.0)).xyw;
#if defined(VULKAN)
    g_vtx_pos_cs_prev.y = -g_vtx_pos_cs_prev.y;
#endif
#endif // MOVING_PERM
}
