#version 430 core
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "texturing_common.glsl"
#include "vegetation_common.glsl"
#include "_vs_instance_index_emu.glsl"

#pragma multi_compile _ VEGETATION
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef VEGETATION
layout(location = VTX_AUX_LOC) in uint g_in_vtx_uvs1_packed;
#endif // VEGETATION

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;
layout(binding = BIND_NOISE_TEX) uniform sampler2D g_noise_tex;

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    material_data_t g_materials[];
};

#if defined(NO_BINDLESS)
layout(binding = BIND_MAT_TEX4) uniform sampler2D g_pp_pos_tex;
layout(binding = BIND_MAT_TEX5) uniform sampler2D g_pp_dir_tex;
#endif

invariant gl_Position;

void main() {
    const ivec2 instance = g_instance_indices[gl_InstanceIndex];

    const mat4 model_matrix = FetchModelMatrix(g_instances_buf, instance.x);
    vec3 vtx_pos_ls = g_in_vtx_pos;

#ifdef VEGETATION
    const material_data_t mat = g_materials[instance.y];

    // load vegetation properties
    vec4 veg_params = texelFetch(g_instances_buf, instance.x * INSTANCE_BUF_STRIDE + 3);
    vec2 pp_vtx_uvs = unpackHalf2x16(g_in_vtx_uvs1_packed);

#if !defined(NO_BINDLESS)
    TEX_HANDLE g_pp_pos_tex = GET_HANDLE(mat.texture_indices[4]);
    TEX_HANDLE g_pp_dir_tex = GET_HANDLE(mat.texture_indices[5]);
#endif // !NO_BINDLESS
    HierarchyData hdata = FetchHierarchyData(g_pp_pos_tex, g_pp_dir_tex, pp_vtx_uvs);

    vec3 obj_pos_ws = model_matrix[3].xyz;
    vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 _unused = vec3(0.0);
    vtx_pos_ls = TransformVegetation(vtx_pos_ls, _unused, _unused, g_noise_tex, wind_scroll, wind_params, wind_vec_ls, hdata);
#endif // VEGETATION

    const vec3 vtx_pos_ws = (model_matrix * vec4(vtx_pos_ls, 1.0)).xyz;
    gl_Position = g_shrd_data.clip_from_world * vec4(vtx_pos_ws, 1.0);
}
