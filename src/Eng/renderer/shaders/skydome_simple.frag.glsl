#version 430 core

#include "_fs_common.glsl"
#include "skydome_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

#define SKY_SUN_BLEND_VAL 0.0005

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;

layout(location = 0) in vec3 g_vtx_pos;

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;

void main() {
    const vec3 view_dir_ws = normalize(g_vtx_pos);

    const vec3 rotated_dir = rotate_xz(view_dir_ws, g_shrd_data.env_col.w);
    vec3 out_color = g_shrd_data.env_col.xyz * texture(g_env_tex, rotated_dir).xyz;

    // sun disk is missing from cubemap, add it here
    if (g_shrd_data.sun_dir.w > 0.0 && view_dir_ws.y > -0.01) {
        const float costh = dot(view_dir_ws, g_shrd_data.sun_dir.xyz);
        const float cos_theta = 1.0 / sqrt(1.0 + g_shrd_data.sun_dir.w * g_shrd_data.sun_dir.w);
        const float sun_disk = smoothstep(cos_theta - SKY_SUN_BLEND_VAL, cos_theta + SKY_SUN_BLEND_VAL, costh);
        out_color += sun_disk * g_shrd_data.sun_col.xyz;
    }

    g_out_color = vec4(compress_hdr(out_color), 1.0);
}
