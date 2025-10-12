#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS) && defined(ALPHATEST)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "texturing_common.glsl"

#pragma multi_compile _ MOVING
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
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs;
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    material_data_t g_materials[];
};

#ifdef MOVING
    layout(location = 0) out vec3 g_vtx_pos_cs_curr;
    layout(location = 1) out vec3 g_vtx_pos_cs_prev;
    layout(location = 2) out vec2 g_vtx_z_vs_curr;
    layout(location = 3) out vec2 g_vtx_z_vs_prev;
#endif // MOVING
#ifdef ALPHATEST
    layout(location = 4) out vec2 g_vtx_uvs;
    layout(location = 5) out vec3 g_vtx_pos_ls;
    layout(location = 6) out flat float g_alpha;
    #if !defined(NO_BINDLESS)
        layout(location = 7) out flat TEX_HANDLE g_alpha_tex;
    #endif // !NO_BINDLESS
#endif // ALPHATEST

invariant gl_Position;

void main() {
    const ivec2 instance = g_instance_indices[gl_InstanceIndex];

    const mat4 model_matrix_curr = FetchModelMatrix(g_instances_buf, instance.x);
#ifdef MOVING
    const mat4 model_matrix_prev = FetchPrevModelMatrix(g_instances_buf, instance.x);
#endif // MOVING

#ifdef ALPHATEST
    g_vtx_uvs = g_in_vtx_uvs;

    const material_data_t mat = g_materials[instance.y];
    g_alpha = 1.0 - mat.params[3].x;
#if !defined(NO_BINDLESS)
    g_alpha_tex = GET_HANDLE(mat.texture_indices[MAT_TEX_ALPHA]);
#endif // NO_BINDLESS
    g_vtx_pos_ls = g_in_vtx_pos;
#endif // ALPHATEST

    const vec3 vtx_pos_ws_curr = (model_matrix_curr * vec4(g_in_vtx_pos, 1.0)).xyz;
    gl_Position = g_shrd_data.clip_from_world * vec4(vtx_pos_ws_curr, 1.0);

#ifdef MOVING
    g_vtx_pos_cs_curr = gl_Position.xyw;

    const vec3 vtx_pos_ws_prev = (model_matrix_prev * vec4(g_in_vtx_pos, 1.0)).xyz;
    g_vtx_pos_cs_prev = (g_shrd_data.prev_clip_from_world * vec4(vtx_pos_ws_prev, 1.0)).xyw;
    const vec4 vtx_pos_vs_curr = g_shrd_data.view_from_world * vec4(vtx_pos_ws_curr, 1.0);
    const vec4 vtx_pos_vs_prev = g_shrd_data.prev_view_from_world * vec4(vtx_pos_ws_prev, 1.0);
    g_vtx_z_vs_curr = vtx_pos_vs_curr.zw;
    g_vtx_z_vs_prev = vtx_pos_vs_prev.zw;
#endif // MOVING
}
