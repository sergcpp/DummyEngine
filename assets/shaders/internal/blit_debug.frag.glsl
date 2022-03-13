#version 310 es
#extension GL_EXT_texture_buffer : require
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

/*
PERM @MSAA_4
*/

#if defined(MSAA_4)
layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2DMS g_texture;
#else
layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2D g_texture;
#endif
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buffer;

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

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    float depth = texelFetch(g_texture, ivec2(g_vtx_uvs), 0).r;
    depth = g_clip_info[0] / (depth * (g_clip_info[1] - g_clip_info[2]) + g_clip_info[2]);

    float k = log2(depth / g_clip_info[1]) / g_clip_info[3];
    int slice = int(floor(k * float(REN_GRID_RES_Z)));

    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / g_res.y) * REN_GRID_RES_X + (ix * REN_GRID_RES_X / g_res.x);

    highp uvec2 cell_data = texelFetch(g_cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    if (g_mode < 1.0) {
        g_out_color = vec4(heatmap(float(offset_and_lcount.y) * (1.0 / 48.0)), 0.85);
    } else if (g_mode < 2.0) {
        g_out_color = vec4(heatmap(float(dcount_and_pcount.x) * (1.0 / 8.0)), 0.85);
    }

    int xy_cell = (iy * REN_GRID_RES_Y / g_res.y) * REN_GRID_RES_X + ix * REN_GRID_RES_X / g_res.x;
    int xy_cell_right = (iy * REN_GRID_RES_Y / g_res.y) * REN_GRID_RES_X + (ix + 1) * REN_GRID_RES_X / g_res.x;
    int xy_cell_up = ((iy + 1) * REN_GRID_RES_Y / g_res.y) * REN_GRID_RES_X + ix * REN_GRID_RES_X / g_res.x;

    // mark cell border
    if (xy_cell_right != xy_cell || xy_cell_up != xy_cell) {
        g_out_color = vec4(1.0, 0.0, 0.0, 1.0);
    }
}
