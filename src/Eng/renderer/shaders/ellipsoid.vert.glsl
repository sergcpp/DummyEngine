#version 430 core

#include "_vs_common.glsl"
#include "ellipsoid_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    mat4 g_mmatrix;
};
#else
layout(location = U_M_MATRIX_LOC) uniform mat4 g_mmatrix;
#endif

layout(location = 0) out vec3 g_vtx_pos;

void main() {
    vec3 vertex_position_ws = (g_mmatrix * vec4(g_in_vtx_pos, 1.0)).xyz;
    g_vtx_pos = vertex_position_ws;

    gl_Position = g_shrd_data.clip_from_world * g_mmatrix * vec4(g_in_vtx_pos, 1.0);
}
