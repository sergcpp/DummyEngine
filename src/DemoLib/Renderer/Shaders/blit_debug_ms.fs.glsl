R"(
#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
    precision mediump float;
#endif

#define GRID_RES_X )" AS_STR(REN_GRID_RES_X) R"(
#define GRID_RES_Y )" AS_STR(REN_GRID_RES_Y) R"(
#define GRID_RES_Z )" AS_STR(REN_GRID_RES_Z) R"(
        
layout(binding = 0) uniform mediump sampler2DMS s_texture;
layout(binding = 14) uniform highp usamplerBuffer cells_buffer;
layout(binding = 15) uniform highp usamplerBuffer items_buffer;

layout(location = 16) uniform ivec2 res;
layout(location = 17) uniform int mode;

in vec2 aVertexUVs_;

out vec4 outColor;

vec3 heatmap(float t) {
    vec3 r = vec3(t) * 2.1 - vec3(1.8, 1.14, 0.3);
    return vec3(1.0) - r * r;
}

void main() {
    const float n = 0.5;
    const float f = 10000.0;

    float depth = texelFetch(s_texture, ivec2(aVertexUVs_), 0).r;
    depth = 2.0 * depth - 1.0;
    depth = 2.0 * n * f / (f + n - depth * (f - n));
    
    float k = log2(depth / n) / log2(0.0 + f / n);
    int slice = int(floor(k * 24.0));
    
    int ix = int(gl_FragCoord.x);
    int iy = int(gl_FragCoord.y);
    int cell_index = slice * GRID_RES_X * GRID_RES_Y + (iy * GRID_RES_Y / res.y) * GRID_RES_X + ix * GRID_RES_X / res.x;
    
    uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
    uvec2 offset_and_lcount = uvec2(cell_data.x & 0x00ffffffu, cell_data.x >> 24);
    uvec2 dcount_and_pcount = uvec2(cell_data.y & 0x000000ffu, 0);

    if (mode == 0) {
        outColor = vec4(heatmap(float(offset_and_lcount.y) * (1.0 / 16.0)), 0.85);
    } else if (mode == 1) {
        outColor = vec4(heatmap(float(dcount_and_pcount.x) * (1.0 / 8.0)), 0.85);
    }

    int xy_cell = (iy * GRID_RES_Y / res.y) * GRID_RES_X + ix * GRID_RES_X / res.x;
    int xy_cell_right = (iy * GRID_RES_Y / res.y) * GRID_RES_X + (ix + 1) * GRID_RES_X / res.x;
    int xy_cell_up = ((iy + 1) * GRID_RES_Y / res.y) * GRID_RES_X + ix * GRID_RES_X / res.x;

    if (xy_cell_right != xy_cell || xy_cell_up != xy_cell) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
}
)"