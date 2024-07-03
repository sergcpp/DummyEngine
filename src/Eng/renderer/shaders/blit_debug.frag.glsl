#version 430 core
#extension GL_ARB_texture_multisample : enable

#include "_fs_common.glsl"

layout(binding = BIND_BASE0_TEX) uniform sampler2D g_tex;
layout(binding = BIND_CELLS_BUF) uniform usamplerBuffer g_cells_buf;
layout(binding = BIND_ITEMS_BUF) uniform usamplerBuffer g_items_buf;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) vec4 g_clip_info;
                        ivec2 g_res;
                        float g_mode;
};
#else
layout(location = 17) uniform vec4 g_clip_info;
layout(location = 15) uniform ivec2 g_res;
layout(location = 16) uniform float g_mode;
#endif

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

void main() {
    float depth = texelFetch(g_tex, ivec2(g_vtx_uvs), 0).r;
    depth = g_clip_info[0] / (depth * (g_clip_info[1] - g_clip_info[2]) + g_clip_info[2]);

    float k = log2(depth / g_clip_info[1]) / g_clip_info[3];
    int slice = clamp(int(k * float(ITEM_GRID_RES_Z)), 0, ITEM_GRID_RES_Z - 1);

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = slice * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + (iy * ITEM_GRID_RES_Y / g_res.y) * ITEM_GRID_RES_X + (ix * ITEM_GRID_RES_X / g_res.x);

    uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    if (g_mode < 1.0) {
        g_out_color = vec4(heatmap(float(offset_and_lcount.y) * (1.0 / 48.0)), 0.85);
    } else if (g_mode < 2.0) {
        g_out_color = vec4(heatmap(float(dcount_and_pcount.x) * (1.0 / 8.0)), 0.85);
    }

    int xy_cell = (iy * ITEM_GRID_RES_Y / g_res.y) * ITEM_GRID_RES_X + ix * ITEM_GRID_RES_X / g_res.x;
    int xy_cell_right = (iy * ITEM_GRID_RES_Y / g_res.y) * ITEM_GRID_RES_X + (ix + 1) * ITEM_GRID_RES_X / g_res.x;
    int xy_cell_up = ((iy + 1) * ITEM_GRID_RES_Y / g_res.y) * ITEM_GRID_RES_X + ix * ITEM_GRID_RES_X / g_res.x;

    // mark cell border
    if (xy_cell_right != xy_cell || xy_cell_up != xy_cell) {
        g_out_color = vec4(1.0, 0.0, 0.0, 1.0);
    }
}
