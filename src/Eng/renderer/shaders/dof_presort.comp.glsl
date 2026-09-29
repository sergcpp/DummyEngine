#version 430 core

#include "_cs_common.glsl"
#include "dof_interface.h"
#include "dof_common.glsl"
#include "pmj_common.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = COC_TILES_TEX_SLOT) uniform sampler2D g_coc_tiles_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = COLOR_TEX_SLOT) uniform sampler2D g_color_tex;
layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_presort_img;
layout(binding = OUT2_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_downsampled_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

vec2 depthComp2(float depth, float closestDepth, float midTile) {
    const float DOF_SCALE_FOREGROUND = 1.0 / midTile;
    const float d = DOF_SCALE_FOREGROUND * (closestDepth - depth);
    const float background = smoothstep(0.0, 1.0, d);
    return vec2(background, 1.0 - background);
}

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const uint seed = get_pixel_seed(icoord, g_params.seed);
    // Hide the visible tiling due to separation plane
    const vec2 offset_tile = (get_scrambled_2d_rand(g_random_seq, DOF_RANDOM_DIM, seed, 0u) - 0.5) * 2.0;

    const vec2 uv = (vec2(icoord) + 0.5) / vec2(g_params.img_size.xy);

    const vec2 tile_grid = vec2(textureSize(g_coc_tiles_tex, 0)) / vec2(g_params.img_size.zw);
    const ivec2 tile_coord = clamp(ivec2(((vec2(icoord) + offset_tile) * 2.0 + 0.5) * tile_grid / TILE_RES), ivec2(0),
                                   ivec2(textureSize(g_coc_tiles_tex, 0).xy) - 1);
    const vec3 tile_fetch = texelFetch(g_coc_tiles_tex, tile_coord, 0).xyz;

    const float this_depth = LinearizeDepth(texture(g_depth_tex, uv).x, g_params.clip_info);
    const float coc = min(MAX_KERNEL_RADIUS, blurRadiusFromDelta(this_depth, FOCAL_DEPTH, COC_FACTOR, FOCAL_LENGTH));

    // Corresponds to the 100 inches in the slides. However, I decided to not leave it as an entirely fixed value
    // to decrease artifacts at indoor scenarios. Then, the midTile is considered, which is almost optimal, but a
    // different separation plane per tile creates artifacts so parameters have to be chosen wisely.
    float mid_tile = min(2.5, (tile_fetch.x - tile_fetch.z) * 0.5);

    const vec2 center_weights = depthComp2(this_depth, tile_fetch.x, mid_tile);
    const float coc_signed = this_depth > FOCAL_DEPTH ? coc : -coc;
    imageStore(g_presort_img, icoord, vec4(coc_signed, SampleAlpha(coc) * center_weights, 1.0));

    const vec4 center_color = texture(g_color_tex, uv);
    vec4 foreground = vec4(center_color.xyz, 1.0);
    vec4 background = foreground;

    // This denominator should be 3.0 to cover the in-ring-holes.
    const float blur_radius = max(1.0, coc / NUM_DOF_RINGS);
    const mat2 rotation = mat2(g_params.rot.xy, g_params.rot.zw);

    // See slides 109, 110, 115
    for (int i = 0; i < 8; ++i) {
        const vec2 fetch_pos = uv + (rotation * spiral8Pi3[i]) * blur_radius / vec2(g_params.img_size.zw);
        const vec3 color_read = texture(g_color_tex, fetch_pos).xyz;
        const float sample_depth = LinearizeDepth(texture(g_depth_tex, fetch_pos).x, g_params.clip_info);
        const vec2 sample_weights = depthComp2(sample_depth, tile_fetch.x, mid_tile);
        // The Karis-Average as mentioned in the CoD slides is not necessary due to the temporal stabilization
        foreground += sample_weights.y * vec4(color_read, 1.0);
        background += sample_weights.x * vec4(color_read, 1.0);
    }

    const vec4 assembled_blur = mix(foreground, background, center_weights.x);
    imageStore(g_downsampled_img, icoord, vec4(assembled_blur.xyz / assembled_blur.w, coc_signed));
}
