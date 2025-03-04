#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "texturing_common.glsl"
#include "vegetation_common.glsl"

#include "shadow_interface.h"

#pragma multi_compile _ ALPHATEST
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef ALPHATEST
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs;
#endif
layout(location = VTX_AUX_LOC) in uint g_in_vtx_uvs1_packed;

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;
layout(binding = BIND_NOISE_TEX) uniform sampler2D g_noise_tex;

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    mat4 g_shadow_view_proj_mat;
};
#else // VULKAN
layout(location = U_M_MATRIX_LOC) uniform mat4 g_shadow_view_proj_mat;
#endif // VULKAN

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#if defined(NO_BINDLESS)
layout(binding = BIND_MAT_TEX4) uniform sampler2D g_pp_pos_tex;
layout(binding = BIND_MAT_TEX5) uniform sampler2D g_pp_dir_tex;
#endif

#ifdef ALPHATEST
    layout(location = 0) out vec2 g_vtx_uvs;
    layout(location = 1) out flat float g_alpha;
    #if !defined(NO_BINDLESS)
        layout(location = 2) out flat TEX_HANDLE g_alpha_tex;
    #endif // !NO_BINDLESS
#endif // ALPHATEST

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];
    mat4 MMatrix = FetchModelMatrix(g_instances_buf, instance.x);

    vec4 veg_params = texelFetch(g_instances_buf, instance.x * INSTANCE_BUF_STRIDE + 3);
    vec2 pp_vtx_uvs = unpackHalf2x16(g_in_vtx_uvs1_packed);

    MaterialData mat = g_materials[instance.y];
#if !defined(NO_BINDLESS)
    TEX_HANDLE g_pp_pos_tex = GET_HANDLE(mat.texture_indices[4]);
    TEX_HANDLE g_pp_dir_tex = GET_HANDLE(mat.texture_indices[5]);
#endif // !NO_BINDLESS
    HierarchyData hdata = FetchHierarchyData(SAMPLER2D(g_pp_pos_tex), SAMPLER2D(g_pp_dir_tex), pp_vtx_uvs);

    vec3 obj_pos_ws = MMatrix[3].xyz;
    vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 _unused;
    vec3 vtx_pos_ls = TransformVegetation(g_in_vtx_pos, _unused, _unused, g_noise_tex, wind_scroll, wind_params, wind_vec_ls, hdata);

    vec3 vtx_pos_ws = (MMatrix * vec4(vtx_pos_ls, 1.0)).xyz;

#ifdef ALPHATEST
    g_vtx_uvs = g_in_vtx_uvs;

    g_alpha = 1.0 - mat.params[3].x;
#if !defined(NO_BINDLESS)
    g_alpha_tex = GET_HANDLE(mat.texture_indices[MAT_TEX_ALPHA]);
#endif // !NO_BINDLESS
#endif // ALPHATEST

    gl_Position = g_shadow_view_proj_mat * vec4(vtx_pos_ws, 1.0);
}
