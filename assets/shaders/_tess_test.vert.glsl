#version 320 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif
//#extension GL_EXT_control_flow_attributes : enable

#include "internal/_vs_common.glsl"
#include "internal/_vs_instance_index_emu.glsl"

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

layout(binding = BIND_INST_BUF) uniform highp samplerBuffer g_instances_buf;

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#if defined(GL_ARB_bindless_texture)
layout(binding = BIND_BINDLESS_TEX) readonly buffer TextureHandles {
    uvec2 texture_handles[];
};
#endif

#if defined(VULKAN)
layout(location = 0) out highp vec3 g_vtx_pos_cs;
layout(location = 1) out mediump vec2 g_vtx_uvs_cs;
layout(location = 2) out mediump vec3 g_vtx_norm_cs;
layout(location = 3) out mediump vec3 g_vtx_tangent_cs;
layout(location = 4) out highp vec3 g_vtx_sh_uvs_cs[4];
#if defined(GL_ARB_bindless_texture)
layout(location = 9) out flat uvec2 g_diff_tex;
layout(location = 10) out flat uvec2 g_norm_tex;
layout(location = 11) out flat uvec2 g_spec_tex;
layout(location = 12) out flat uvec2 g_bump_tex;
#endif // GL_ARB_bindless_texture
#else
out highp vec3 g_vtx_pos_cs;
out mediump vec2 g_vtx_uvs_cs;
out mediump vec3 g_vtx_norm_cs;
out mediump vec3 g_vtx_tangent_cs;
out highp vec3 g_vtx_sh_uvs_cs[4];
#if defined(GL_ARB_bindless_texture)
out flat uvec2 g_diff_tex;
out flat uvec2 g_norm_tex;
out flat uvec2 g_spec_tex;
out flat uvec2 g_bump_tex;
#endif // GL_ARB_bindless_texture
#endif

//invariant gl_Position;

void main(void) {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(g_instances_buf, instance.x);

    vec3 vtx_pos_ws = (model_matrix * vec4(g_in_vtx_pos, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(g_in_vtx_normal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(g_in_vtx_normal.w, g_in_vtx_tangent, 0.0)).xyz);

    g_vtx_pos_cs = vtx_pos_ws;
    g_vtx_norm_cs = vtx_nor_ws;
    g_vtx_tangent_cs = vtx_tan_ws;
    g_vtx_uvs_cs = g_in_vtx_uvs0;

    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );

    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        g_vtx_sh_uvs_cs[i] = (g_shrd_data.shadowmap_regions[i].clip_from_world * vec4(vtx_pos_ws, 1.0)).xyz;
        g_vtx_sh_uvs_cs[i] = 0.5 * g_vtx_sh_uvs_cs[i] + 0.5;
        g_vtx_sh_uvs_cs[i].xy *= vec2(0.25, 0.5);
        g_vtx_sh_uvs_cs[i].xy += offsets[i];
    }

#if defined(GL_ARB_bindless_texture)
    MaterialData mat = g_materials[instance.y];
    g_diff_tex = texture_handles[mat.texture_indices[0]];
    g_norm_tex = texture_handles[mat.texture_indices[1]];
    g_spec_tex = texture_handles[mat.texture_indices[2]];
    g_bump_tex = texture_handles[mat.texture_indices[3]];
#endif // GL_ARB_bindless_texture
}
