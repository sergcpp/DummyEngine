#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "_texturing.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
PERM @VEGETATION
*/

layout(location = REN_VTX_POS_LOC) in vec3 g_in_vtx_pos;
layout(location = REN_VTX_NOR_LOC) in vec4 g_in_vtx_normal;
layout(location = REN_VTX_TAN_LOC) in vec2 g_in_vtx_tangent;
layout(location = REN_VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#ifdef VEGETATION
layout(location = REN_VTX_AUX_LOC) in uint g_in_vtx_color_packed;
#endif // VEGETATION

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
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 4) out flat TEX_HANDLE g_diff_texture;
    LAYOUT(location = 5) out flat TEX_HANDLE g_norm_texture;
    LAYOUT(location = 6) out flat TEX_HANDLE g_spec_texture;
#endif // BINDLESS_TEXTURES

invariant gl_Position;

void main(void) {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(g_instances_buffer, instance.x);

#ifdef VEGETATION
    // load vegetation properties
    vec4 veg_params = texelFetch(g_instances_buffer, instance.x * INSTANCE_BUF_STRIDE + 3);

    vec4 vtx_color = unpackUnorm4x8(g_in_vtx_color_packed);

    vec3 obj_pos_ws = model_matrix[3].xyz;
    vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 vtx_pos_ls = TransformVegetation(g_in_vtx_pos, vtx_color, wind_scroll, wind_params, wind_vec_ls, g_noise_texture);
#else
    vec3 vtx_pos_ls = g_in_vtx_pos;
#endif // VEGETATION

    vec3 vtx_pos_ws = (model_matrix * vec4(vtx_pos_ls, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(g_in_vtx_normal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(g_in_vtx_normal.w, g_in_vtx_tangent, 0.0)).xyz);

    g_vtx_pos = vtx_pos_ws;
    g_vtx_normal = vtx_nor_ws;
    g_vtx_tangent = vtx_tan_ws;
    g_vtx_uvs = g_in_vtx_uvs0;

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    g_diff_texture = GET_HANDLE(mat.texture_indices[0]);
    g_norm_texture = GET_HANDLE(mat.texture_indices[1]);
    g_spec_texture = GET_HANDLE(mat.texture_indices[2]);
#endif // BINDLESS_TEXTURES

    gl_Position = g_shrd_data.view_proj_matrix * vec4(vtx_pos_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
