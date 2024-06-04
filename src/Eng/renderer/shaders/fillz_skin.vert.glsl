#version 320 es
#extension GL_EXT_texture_buffer : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "_texturing.glsl"

#pragma multi_compile _ MOVING
#pragma multi_compile _ OUTPUT_VELOCITY
#pragma multi_compile _ TRANSPARENT

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos_curr;
#ifdef TRANSPARENT
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#endif
#ifdef OUTPUT_VELOCITY
layout(location = VTX_PRE_LOC) in vec3 g_in_vtx_pos_prev;
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

#ifdef OUTPUT_VELOCITY
    layout(location = 0) out vec3 g_vtx_pos_cs_curr;
    layout(location = 1) out vec3 g_vtx_pos_cs_prev;
#endif // OUTPUT_VELOCITY
#ifdef TRANSPARENT
    layout(location = 2) out vec2 g_vtx_uvs0;
    layout(location = 3) out vec3 g_vtx_pos_ls;
    #if defined(BINDLESS_TEXTURES)
        layout(location = 4) out flat TEX_HANDLE g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT

invariant gl_Position;

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];
    mat4 model_matrix_curr = FetchModelMatrix(g_instances_buf, instance.x);

#ifdef MOVING
    mat4 model_matrix_prev = FetchPrevModelMatrix(g_instances_buf, instance.x);
#endif

#ifdef TRANSPARENT
    g_vtx_uvs0 = g_in_vtx_uvs0;

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    g_alpha_tex = GET_HANDLE(mat.texture_indices[3]);
#endif // BINDLESS_TEXTURES
    g_vtx_pos_ls = g_in_vtx_pos_curr;
#endif

    vec3 vtx_pos_ws_curr = (model_matrix_curr * vec4(g_in_vtx_pos_curr, 1.0)).xyz;
    gl_Position = g_shrd_data.clip_from_world_no_translation * vec4(vtx_pos_ws_curr- g_shrd_data.cam_pos_and_exp.xyz, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif

#ifdef OUTPUT_VELOCITY
    g_vtx_pos_cs_curr = gl_Position.xyw;
#ifdef MOVING
    vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(g_in_vtx_pos_prev, 1.0)).xyz;
#else
    vec3 vtx_pos_ws_prev = (model_matrix_curr * vec4(g_in_vtx_pos_prev, 1.0)).xyz;
#endif
    g_vtx_pos_cs_prev = (g_shrd_data.prev_clip_from_world_no_translation * vec4(vtx_pos_ws_prev - g_shrd_data.prev_cam_pos.xyz, 1.0)).xyw;
#if defined(VULKAN)
    g_vtx_pos_cs_prev.y = -g_vtx_pos_cs_prev.y;
#endif
#endif
}

