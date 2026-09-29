#version 430 core

#include "_cs_common.glsl"
#include "dof_interface.h"
#include "dof_common.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DOWNSAMPLED_TEX_SLOT) uniform sampler2D g_source_tex;
layout(binding = COC_TILES_TEX_SLOT) uniform sampler2D g_coc_tiles_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

// Source: http://casual-effects.com/research/McGuire2008Median/index.html
#define s2(a, b)        temp = a; a = min(a, b); b = max(temp, b);
#define mn3(a, b, c)    s2(a, b); s2(a, c);
#define mx3(a, b, c)    s2(b, c); s2(a, c);

#define mnmx3(a, b, c)                mx3(a, b, c); s2(a, b);                                   // 3 exchanges
#define mnmx4(a, b, c, d)             s2(a, b); s2(c, d); s2(a, c); s2(b, d);                   // 4 exchanges
#define mnmx5(a, b, c, d, e)          s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e);           // 6 exchanges
#define mnmx6(a, b, c, d, e, f)       s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const ivec2 source_size = ivec2(textureSize(g_source_tex, 0).xy);
    const ivec2 tiles_size = ivec2(textureSize(g_coc_tiles_tex, 0).xy);
    // Map halfres pixel to the tile that covers it in fullres space (g_params.img_size.zw)
    const vec2 full_pos = (vec2(icoord) + 0.5) * (vec2(g_params.img_size.zw) / vec2(g_params.img_size.xy));
    const ivec2 tile_coord = clamp(ivec2(full_pos / TILE_RES), ivec2(0), tiles_size - 1);
    const vec3 coc_fetch = texelFetch(g_coc_tiles_tex, tile_coord, 0).xyz;

    // Early out if max radius is smaller than our 3x3 median window
    if (coc_fetch.y < EARLY_OUT_MAX_RADIUS) {
        imageStore(g_out_img, icoord, texelFetch(g_source_tex, clamp(icoord, ivec2(0), source_size - 1), 0));
        return;
    }

    vec4 v[6];
    v[0] = texelFetch(g_source_tex, clamp(icoord + ivec2(-1, -1), ivec2(0), source_size - 1), 0);
    v[1] = texelFetch(g_source_tex, clamp(icoord + ivec2(0, -1), ivec2(0), source_size - 1), 0);
    v[2] = texelFetch(g_source_tex, clamp(icoord + ivec2(1, -1), ivec2(0), source_size - 1), 0);
    v[3] = texelFetch(g_source_tex, clamp(icoord + ivec2(-1, 0), ivec2(0), source_size - 1), 0);
    v[4] = texelFetch(g_source_tex, clamp(icoord + ivec2(0, 0), ivec2(0), source_size - 1), 0);
    v[5] = texelFetch(g_source_tex, clamp(icoord + ivec2(1, 0), ivec2(0), source_size - 1), 0);

    // Starting with a subset of size 6, remove the min and max each time
    vec4 temp;
    mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]);

    v[5] = texelFetch(g_source_tex, clamp(icoord + ivec2(-1, 1), ivec2(0), source_size - 1), 0);

    mnmx5(v[1], v[2], v[3], v[4], v[5]);

    v[5] = texelFetch(g_source_tex, clamp(icoord + ivec2(0, 1), ivec2(0), source_size - 1), 0);

    mnmx4(v[2], v[3], v[4], v[5]);

    v[5] = texelFetch(g_source_tex, clamp(icoord + ivec2(1, 1), ivec2(0), source_size - 1), 0);

    mnmx3(v[3], v[4], v[5]);

    imageStore(g_out_img, icoord, v[4]);
}
