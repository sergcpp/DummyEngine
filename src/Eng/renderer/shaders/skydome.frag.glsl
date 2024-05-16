#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "skydome_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(location = 0) in highp vec3 g_vtx_pos;

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;

void main() {
    const vec3 view_dir_ws = normalize(g_vtx_pos - g_shrd_data.cam_pos_and_gamma.xyz);

    const vec3 rotated_dir = rotate_xz(view_dir_ws, g_shrd_data.env_col.w);
    g_out_color.rgb = g_shrd_data.env_col.xyz * texture(g_env_tex, rotated_dir).rgb;
    g_out_color.a = 1.0;
}
