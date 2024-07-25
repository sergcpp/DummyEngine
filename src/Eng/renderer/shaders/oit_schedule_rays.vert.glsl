#version 430 core
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "vegetation_common.glsl"
#include "texturing_common.glsl"

#pragma multi_compile _ VEGETATION
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
layout(location = VTX_NOR_LOC) in vec4 g_in_vtx_normal;
layout(location = VTX_TAN_LOC) in vec2 g_in_vtx_tangent;
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#ifdef VEGETATION
layout(location = VTX_AUX_LOC) in uint g_in_vtx_uvs1_packed;
#endif // VEGETATION

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

#if defined(NO_BINDLESS)
layout(binding = BIND_MAT_TEX4) uniform sampler2D g_pp_pos_tex;
layout(binding = BIND_MAT_TEX5) uniform sampler2D g_pp_dir_tex;
#endif

layout(location = 0) out vec3 g_vtx_pos;
layout(location = 1) out vec2 g_vtx_uvs;
layout(location = 2) out vec3 g_vtx_normal;
layout(location = 3) out vec3 g_vtx_tangent;
#if !defined(NO_BINDLESS)
    layout(location = 4) out flat TEX_HANDLE g_norm_tex;
    layout(location = 5) out flat TEX_HANDLE g_alpha_tex;
#endif // !NO_BINDLESS
layout(location = 6) out flat vec4 g_mat_params0;
layout(location = 7) out flat vec4 g_mat_params2;

invariant gl_Position;

void main() {
    const ivec2 instance = g_instance_indices[gl_InstanceIndex];

    const mat4 model_matrix = FetchModelMatrix(g_instances_buf, instance.x);
    vec3 vtx_pos_ls = g_in_vtx_pos;
    vec3 vtx_nor_ls = g_in_vtx_normal.xyz, vtx_tan_ls = vec3(g_in_vtx_normal.w, g_in_vtx_tangent);

    const MaterialData mat = g_materials[instance.y];
#ifdef VEGETATION
    // load vegetation properties
    vec4 veg_params = texelFetch(g_instances_buf, instance.x * INSTANCE_BUF_STRIDE + 3);
    vec2 pp_vtx_uvs = unpackHalf2x16(g_in_vtx_uvs1_packed);

#if !defined(NO_BINDLESS)
    TEX_HANDLE g_pp_pos_tex = GET_HANDLE(mat.texture_indices[4]);
    TEX_HANDLE g_pp_dir_tex = GET_HANDLE(mat.texture_indices[5]);
#endif // !NO_BINDLESS
    HierarchyData hdata = FetchHierarchyData(SAMPLER2D(g_pp_pos_tex), SAMPLER2D(g_pp_dir_tex), pp_vtx_uvs);

    vec3 obj_pos_ws = model_matrix[3].xyz;
    vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 _unused = vec3(0.0);
    vtx_pos_ls = TransformVegetation(vtx_pos_ls, _unused, _unused, g_noise_tex, wind_scroll, wind_params, wind_vec_ls, hdata);
#endif // VEGETATION

    const vec3 vtx_pos_ws = (model_matrix * vec4(vtx_pos_ls, 1.0)).xyz;
    const vec3 vtx_nor_ws = normalize((model_matrix * vec4(vtx_nor_ls, 0.0)).xyz);
    const vec3 vtx_tan_ws = normalize((model_matrix * vec4(vtx_tan_ls, 0.0)).xyz);

    g_vtx_pos = vtx_pos_ws;
    g_vtx_normal = vtx_nor_ws;
    g_vtx_tangent = vtx_tan_ws;
    g_vtx_uvs = g_in_vtx_uvs0;

#if !defined(NO_BINDLESS)
    g_norm_tex = GET_HANDLE(mat.texture_indices[1]);
    g_alpha_tex = GET_HANDLE(mat.texture_indices[4]);
#endif // !NO_BINDLESS

    g_mat_params0 = mat.params[1];
    g_mat_params2 = mat.params[3];

    gl_Position = g_shrd_data.clip_from_world * vec4(vtx_pos_ws, 1.0);
}
