#version 430 core

#include "_vs_common.glsl"
#include "gi_cache_common.glsl"
#include "probe_debug_interface.h"

#if !defined(VULKAN)
#define gl_InstanceIndex gl_InstanceID
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(location = VTX_POS_LOC) in vec3 g_in_vtx_pos;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

layout(location = 0) out vec3 g_vtx_pos;
layout(location = 1) out flat vec3 g_probe_center;
layout(location = 2) out flat int g_probe_index;
layout(location = 3) out flat float g_probe_state;

void main() {
    const int probe_index = gl_InstanceIndex;
    g_probe_index = probe_index;

    const ivec3 probe_coords = get_probe_coords(probe_index);
    const vec3 probe_center = get_probe_pos_ws(probe_coords, g_params.grid_scroll.xyz, g_params.grid_origin.xyz, g_params.grid_spacing.xyz, g_offset_tex);
    g_probe_center = probe_center;

    const int scroll_probe_index = get_scrolling_probe_index(probe_coords, g_params.grid_scroll.xyz);
    const ivec3 scroll_tex_coords = get_probe_texel_coords(scroll_probe_index);
    g_probe_state = texelFetch(g_offset_tex, scroll_tex_coords, 0).w;

    const float scale = 0.1 * min(g_params.grid_spacing.x, min(g_params.grid_spacing.y, g_params.grid_spacing.z));
    const vec3 vertex_position_ws = probe_center + scale * g_in_vtx_pos;
    g_vtx_pos = vertex_position_ws;

    gl_Position = g_shrd_data.clip_from_world * vec4(vertex_position_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
