#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "internal/_vs_common.glsl"
#include "internal/_vs_instance_index_emu.glsl"
#include "internal/_texturing.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(location = REN_VTX_POS_LOC) in vec3 g_in_vtx_pos;
layout(location = REN_VTX_NOR_LOC) in vec4 g_in_vtx_normal;
layout(location = REN_VTX_TAN_LOC) in vec2 g_in_vtx_tangent;
layout(location = REN_VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
layout(location = REN_VTX_AUX_LOC) in uint g_vtx_unused;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = REN_INST_INDICES_BUF_SLOT, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer g_instances_buf;
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;

layout(binding = REN_MATERIALS_SLOT, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

LAYOUT(location = 0) out highp vec3 g_vtx_pos;
LAYOUT(location = 1) out mediump vec2 g_vtx_uvs;
LAYOUT(location = 2) out mediump vec3 g_vtx_normal;
LAYOUT(location = 3) out mediump vec3 g_vtx_tangent;
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 8) out flat TEX_HANDLE g_diff_tex;
    LAYOUT(location = 9) out flat TEX_HANDLE g_norm_tex;
    LAYOUT(location = 10) out flat TEX_HANDLE g_spec_tex;
#endif // GL_ARB_bindless_texture

invariant gl_Position;

void main(void) {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(g_instances_buf, instance.x);

    vec3 vtx_pos_ws = (model_matrix * vec4(g_in_vtx_pos, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(g_in_vtx_normal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(g_in_vtx_normal.w, g_in_vtx_tangent, 0.0)).xyz);

    g_vtx_pos = vtx_pos_ws;
    g_vtx_normal = vtx_nor_ws;
    g_vtx_tangent = vtx_tan_ws;
    g_vtx_uvs = g_in_vtx_uvs0;

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    g_diff_tex = GET_HANDLE(mat.texture_indices[0]);
    g_norm_tex = GET_HANDLE(mat.texture_indices[1]);
    g_spec_tex = GET_HANDLE(mat.texture_indices[2]);
#endif // GL_ARB_bindless_texture

    gl_Position = g_shrd_data.view_proj_no_translation * vec4(vtx_pos_ws - g_shrd_data.cam_pos_and_gamma.xyz, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
