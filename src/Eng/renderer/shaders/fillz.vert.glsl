#version 320 es
#extension GL_EXT_texture_buffer : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "_texturing.glsl"

#pragma multi_compile _ MOVING_PERM
#pragma multi_compile _ TRANSPARENT_PERM
#pragma multi_compile _ HASHED_TRANSPARENCY

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef TRANSPARENT_PERM
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#ifdef MOVING_PERM
    layout(location = 0) out vec3 g_vtx_pos_cs_curr;
    layout(location = 1) out vec3 g_vtx_pos_cs_prev;
#endif // MOVING_PERM
#ifdef TRANSPARENT_PERM
    layout(location = 2) out vec2 g_vtx_uvs0;
    #ifdef HASHED_TRANSPARENCY
        layout(location = 3) out vec3 g_vtx_pos_ls;
    #endif // HASHED_TRANSPARENCY
    #if defined(BINDLESS_TEXTURES)
        layout(location = 4) out flat TEX_HANDLE g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

invariant gl_Position;

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];

    mat4 model_matrix_curr = FetchModelMatrix(g_instances_buf, instance.x);
#ifdef MOVING_PERM
    mat4 model_matrix_prev = FetchPrevModelMatrix(g_instances_buf, instance.x);
#endif // MOVING_PERM

#ifdef TRANSPARENT_PERM
    g_vtx_uvs0 = g_in_vtx_uvs0;

    MaterialData mat = g_materials[instance.y];
#if defined(BINDLESS_TEXTURES)
    g_alpha_tex = GET_HANDLE(mat.texture_indices[4]);
#endif // BINDLESS_TEXTURES
#ifdef HASHED_TRANSPARENCY
    g_vtx_pos_ls = g_in_vtx_pos;
#endif // HASHED_TRANSPARENCY
#endif // TRANSPARENT_PERM

    vec3 vtx_pos_ws_curr = (model_matrix_curr * vec4(g_in_vtx_pos, 1.0)).xyz;
    gl_Position = g_shrd_data.clip_from_world_no_translation * vec4(vtx_pos_ws_curr - g_shrd_data.cam_pos_and_gamma.xyz, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif

#ifdef MOVING_PERM
    g_vtx_pos_cs_curr = gl_Position.xyw;

    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(g_in_vtx_pos, 1.0)).xyz;
    g_vtx_pos_cs_prev = (g_shrd_data.prev_clip_from_world_no_translation * vec4(vtx_pos_ws_prev - g_shrd_data.prev_cam_pos.xyz, 1.0)).xyw;
#if defined(VULKAN)
    g_vtx_pos_cs_prev.y = -g_vtx_pos_cs_prev.y;
#endif
#endif // MOVING_PERM
}
