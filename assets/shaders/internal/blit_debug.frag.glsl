#version 310 es
#extension GL_EXT_texture_buffer : require
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

/*
PERM @MSAA_4
*/

#if defined(MSAA_4)
layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2DMS s_texture;
#else
layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2D s_texture;
#endif
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;

layout(location = 15) uniform ivec2 res;
layout(location = 16) uniform int mode;
layout(location = 17) uniform vec4 uClipInfo;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    float depth = texelFetch(s_texture, ivec2(aVertexUVs_), 0).r;
    depth = uClipInfo[0] / (depth * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
    
    float k = log2(depth / uClipInfo[1]) / uClipInfo[3];
    int slice = int(floor(k * float(REN_GRID_RES_Z)));
    
    int ix = int(gl_FragCoord.x), iy = int(gl_FragCoord.y);
    int cell_index = slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / res.y) * REN_GRID_RES_X + (ix * REN_GRID_RES_X / res.x);
    
    highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    if (mode == 0) {
        outColor = vec4(heatmap(float(offset_and_lcount.y) * (1.0 / 48.0)), 0.85);
    } else if (mode == 1) {
        outColor = vec4(heatmap(float(dcount_and_pcount.x) * (1.0 / 8.0)), 0.85);
    }

    int xy_cell = (iy * REN_GRID_RES_Y / res.y) * REN_GRID_RES_X + ix * REN_GRID_RES_X / res.x;
    int xy_cell_right = (iy * REN_GRID_RES_Y / res.y) * REN_GRID_RES_X + (ix + 1) * REN_GRID_RES_X / res.x;
    int xy_cell_up = ((iy + 1) * REN_GRID_RES_Y / res.y) * REN_GRID_RES_X + ix * REN_GRID_RES_X / res.x;

    // mark cell border
    if (xy_cell_right != xy_cell || xy_cell_up != xy_cell) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
}
