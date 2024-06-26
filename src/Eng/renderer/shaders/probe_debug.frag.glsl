#version 430 core

#include "_fs_common.glsl"
#include "gi_cache_common.glsl"
#include "probe_debug_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;

layout(location = 0) in vec3 g_vtx_pos;
layout(location = 1) in flat vec3 g_probe_center;
layout(location = 2) in flat int g_probe_index;
layout(location = 3) in flat float g_probe_state;

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;

void main() {
    const vec3 dir = normalize(g_vtx_pos - g_probe_center);
    const vec2 uv = get_oct_coords(dir);

    const vec3 probe_uv = get_probe_uv(g_probe_index, uv, PROBE_IRRADIANCE_RES - 2);
    vec3 irradiance = textureLod(g_irradiance_tex, probe_uv, 0.0).rgb;
    if (g_probe_state < 0.5) {
        irradiance.x = 1.0;
    }

    g_out_color = vec4(compress_hdr(irradiance), 1.0);
}
