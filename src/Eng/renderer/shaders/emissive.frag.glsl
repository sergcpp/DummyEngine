#version 430 core
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_fs_common.glsl"
#include "texturing_common.glsl"

#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

#if defined(NO_BINDLESS)
    layout(binding = BIND_MAT_TEX3) uniform sampler2D g_emission_tex;
#endif // !NO_BINDLESS

layout(location = 0) in vec2 g_vtx_uvs;
#if !defined(NO_BINDLESS)
    layout(location = 1) in flat TEX_HANDLE g_emission_tex;
#endif // !NO_BINDLESS
layout(location = 2) in flat vec4 g_mat_params2;

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;

void main() {
    const vec3 emission_color = g_mat_params2.yzw * SRGBToLinear(YCoCg_to_RGB(textureBindless(g_emission_tex, g_vtx_uvs)));
    g_out_color = vec4(compress_hdr(emission_color, g_shrd_data.cam_pos_and_exp.w), 1.0);
}
