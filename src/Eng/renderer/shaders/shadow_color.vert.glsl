#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_vs_common.glsl"
#include "_vs_instance_index_emu.glsl"
#include "texturing_common.glsl"

#include "shadow_interface.h"

#pragma multi_compile _ ALPHATEST
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs;

layout(binding = BIND_INST_BUF) uniform samplerBuffer g_instances_buf;

layout(binding = BIND_INST_NDX_BUF, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    mat4 g_shadow_view_proj_mat;
};
#else
layout(location = U_M_MATRIX_LOC) uniform mat4 g_shadow_view_proj_mat;
#endif

layout(binding = BIND_MATERIALS_BUF, std430) readonly buffer Materials {
    material_data_t g_materials[];
};

layout(location = 0) out vec2 g_vtx_uvs;
#if !defined(NO_BINDLESS)
    layout(location = 1) out flat TEX_HANDLE g_base_tex;
#endif
layout(location = 2) out flat vec4 g_base_color;
#if !defined(NO_BINDLESS)
    layout(location = 3) out flat TEX_HANDLE g_alpha_tex;
#endif // !NO_BINDLESS
layout(location = 4) out flat float g_alpha;

void main() {
    const ivec2 instance = g_instance_indices[gl_InstanceIndex];
    const mat4 model_matrix = FetchModelMatrix(g_instances_buf, instance.x);

    g_vtx_uvs = g_in_vtx_uvs;

    const material_data_t mat = g_materials[instance.y];
#if !defined(NO_BINDLESS)
    g_base_tex = GET_HANDLE(mat.texture_indices[MAT_TEX_BASECOLOR]);
#endif // !NO_BINDLESS
    g_base_color = vec4(mat.params[0].xyz, mat.params[2].y);

#if !defined(NO_BINDLESS)
    g_alpha_tex = GET_HANDLE(mat.texture_indices[MAT_TEX_ALPHA]);
#endif // !NO_BINDLESS
    g_alpha = 1.0 - mat.params[3].x;

    const vec3 vertex_position_ws = (model_matrix * vec4(g_in_vtx_pos, 1.0)).xyz;
    gl_Position = g_shadow_view_proj_mat * vec4(vertex_position_ws, 1.0);
}
