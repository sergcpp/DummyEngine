#version 310 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "skydome_interface.glsl"

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_texture;

LAYOUT(location = 0) in highp vec3 g_vtx_pos;

layout(location = REN_OUT_COLOR_INDEX) out vec4 g_out_color;
layout(location = REN_OUT_SPEC_INDEX) out vec4 g_out_specular;

void main() {
    vec3 view_dir_ws = normalize(g_vtx_pos - g_shrd_data.cam_pos_and_gamma.xyz);

    g_out_color.rgb = clamp(RGBMDecode(texture(g_env_texture, view_dir_ws)), vec3(0.0), vec3(16.0));
    g_out_color.a = 1.0;
    g_out_specular = vec4(0.0, 0.0, 0.0, 1.0);
}

