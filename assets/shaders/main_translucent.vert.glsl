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

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer g_instances_buffer;
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D g_noise_texture;

layout(binding = REN_MATERIALS_SLOT, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

LAYOUT(location = 0) out highp vec3 g_vtx_pos;
LAYOUT(location = 1) out mediump vec2 g_vtx_uvs;
LAYOUT(location = 2) out mediump vec3 g_vtx_normal;
LAYOUT(location = 3) out mediump vec3 g_vtx_tangent;
LAYOUT(location = 4) out highp vec4 g_vtx_sh_uvs0;
LAYOUT(location = 5) out highp vec4 g_vtx_sh_uvs1;
LAYOUT(location = 6) out highp vec4 g_vtx_sh_uvs2;
#if defined(BINDLESS_TEXTURES)
LAYOUT(location = 7) out flat TEX_HANDLE g_diff_texture;
LAYOUT(location = 8) out flat TEX_HANDLE g_norm_texture;
LAYOUT(location = 9) out flat TEX_HANDLE g_spec_texture;
LAYOUT(location = 10) out flat TEX_HANDLE g_mask_texture;
#endif // BINDLESS_TEXTURES

invariant gl_Position;

void main(void) {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(g_instances_buffer, instance.x);

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
        vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * vec4(vtx_pos_ws, 1.0)).xyz;
#if defined(VULKAN)
        shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
#else // VULKAN
        shadow_uvs = 0.5 * shadow_uvs + 0.5;
#endif // VULKAN
        shadow_uvs.xy *= vec2(0.25, 0.5);
        shadow_uvs.xy += offsets[i];
#if defined(VULKAN)
        shadow_uvs.y = 1.0 - shadow_uvs.y;
#endif // VULKAN
        g_vtx_sh_uvs0[i] = shadow_uvs[0];
        g_vtx_sh_uvs1[i] = shadow_uvs[1];
        g_vtx_sh_uvs2[i] = shadow_uvs[2];
    }

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    g_diff_texture = GET_HANDLE(mat.texture_indices[0]);
    g_norm_texture = GET_HANDLE(mat.texture_indices[1]);
    g_spec_texture = GET_HANDLE(mat.texture_indices[2]);
    g_mask_texture = GET_HANDLE(mat.texture_indices[3]);
#endif // BINDLESS_TEXTURES

    gl_Position = g_shrd_data.view_proj_matrix * vec4(vtx_pos_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
