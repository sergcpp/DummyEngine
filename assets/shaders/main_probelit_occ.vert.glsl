#version 320 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#include "internal/_vs_common.glsl"
#include "internal/_vs_instance_index_emu.glsl"
#include "internal/_texturing.glsl"

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
layout(location = VTX_NOR_LOC) in vec4 g_in_vtx_normal;
layout(location = VTX_TAN_LOC) in vec2 g_in_vtx_tangent;
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
layout(location = VTX_AUX_LOC) in uint g_in_vtx_occlusion;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;
layout(binding = BIND_NOISE_TEX) uniform sampler2D g_noise_tex;

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

layout(location = 0) out highp vec3 g_vtx_pos;
layout(location = 1) out mediump vec2 g_vtx_uvs;
layout(location = 2) out mediump vec3 g_vtx_normal;
layout(location = 3) out mediump vec3 g_vtx_tangent;
layout(location = 4) out mediump vec4 aVertexOcclusion_;
layout(location = 5) out highp vec3 g_vtx_sh_uvs[4];
#if defined(BINDLESS_TEXTURES)
    layout(location = 9) out flat TEX_HANDLE g_diff_tex;
    layout(location = 10) out flat TEX_HANDLE g_norm_tex;
    layout(location = 11) out flat TEX_HANDLE g_spec_tex;
#endif // BINDLESS_TEXTURES

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

    aVertexOcclusion_ = unpackUnorm4x8(g_in_vtx_occlusion);
    aVertexOcclusion_.xyz = 2.0 * aVertexOcclusion_.xyz - vec3(1.0);
    aVertexOcclusion_.xyz = normalize(mat3(g_vtx_tangent,
                                           cross(g_vtx_normal, g_vtx_tangent),
                                           g_vtx_normal) * aVertexOcclusion_.xyz);

    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );

    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        g_vtx_sh_uvs[i] = (g_shrd_data.shadowmap_regions[i].clip_from_world * vec4(vtx_pos_ws, 1.0)).xyz;
        g_vtx_sh_uvs[i] = 0.5 * g_vtx_sh_uvs[i] + 0.5;
        g_vtx_sh_uvs[i].xy *= vec2(0.25, 0.5);
        g_vtx_sh_uvs[i].xy += offsets[i];
    }

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    g_diff_tex = GET_HANDLE(mat.texture_indices[0]);
    g_norm_tex = GET_HANDLE(mat.texture_indices[1]);
    g_spec_tex = GET_HANDLE(mat.texture_indices[2]);
#endif // BINDLESS_TEXTURES

    gl_Position = g_shrd_data.clip_from_world_no_translation * vec4(vtx_pos_ws - g_shrd_data.cam_pos_and_exp.xyz, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
