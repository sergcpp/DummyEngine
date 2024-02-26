#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "skydome_interface.h"

#pragma multi_compile _ COLOR_ONLY

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

LAYOUT(location = 0) in highp vec3 g_vtx_pos;

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;
#if !defined(COLOR_ONLY)
layout(location = LOC_OUT_SPEC) out vec4 g_out_specular;
#endif

void main() {
    vec3 view_dir_ws = normalize(g_vtx_pos - g_shrd_data.cam_pos_and_gamma.xyz);

    g_out_color.rgb = clamp(RGBMDecode(texture(g_env_tex, view_dir_ws)), vec3(0.0), vec3(16.0));
    g_out_color.a = 1.0;
#if !defined(COLOR_ONLY)
    g_out_specular = vec4(0.0, 0.0, 0.0, 1.0);
#endif
}

