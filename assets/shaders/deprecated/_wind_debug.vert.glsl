#version 430 core
//#extension GL_EXT_control_flow_attributes : enable

#include "../internal/_vs_common.glsl"
#include "../internal/_vs_instance_index_emu.glsl"

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
layout(location = VTX_NOR_LOC) in vec4 g_in_vtx_normal;
layout(location = VTX_TAN_LOC) in vec2 g_in_vtx_tangent;
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
layout(location = VTX_AUX_LOC) in vec2 g_vtx_unused;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;

layout(location = 0) out vec3 g_vtx_pos;
layout(location = 1) out vec2 g_vtx_uvs;
layout(location = 2) out vec3 g_vtx_normal;
layout(location = 3) out vec3 g_vtx_tangent;
layout(location = 4) out vec3 g_vtx_sh_uvs[4];

invariant gl_Position;

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(g_instances_buf, instance.x);

    vec3 vtx_pos_ws = (model_matrix * vec4(g_in_vtx_pos, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(g_in_vtx_normal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(g_in_vtx_normal.w, g_in_vtx_tangent, 0.0)).xyz);

    g_vtx_pos = vtx_pos_ws;
    g_vtx_normal = vtx_nor_ws;
    g_vtx_tangent = vtx_tan_ws;
    g_vtx_uvs = g_in_vtx_uvs0;

    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );

    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        g_vtx_sh_uvs[i] = (g_shrd_data.shadowmap_regions[i].clip_from_world *
                                vec4(vtx_pos_ws, 1.0)).xyz;
        g_vtx_sh_uvs[i] = 0.5 * g_vtx_sh_uvs[i] + 0.5;
        g_vtx_sh_uvs[i].xy *= vec2(0.25, 0.5);
        g_vtx_sh_uvs[i].xy += offsets[i];
    }

    gl_Position = g_shrd_data.clip_from_world * vec4(vtx_pos_ws, 1.0);
}
