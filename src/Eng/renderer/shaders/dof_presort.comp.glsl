#version 430 core

#include "_cs_common.glsl"
#include "dof_interface.h"
#include "dof_common.glsl"
#include "pmj_common.glsl"

// Must match dof_filter.comp.glsl: compiled in when the aperture is not circular (camera lens_blades > 2).
#pragma multi_compile _ LENS_SHAPE

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = COC_TILES_TEX_SLOT) uniform sampler2D g_coc_tiles_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = COLOR_TEX_SLOT) uniform sampler2D g_color_tex;
layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;
layout(binding = VEL_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = PRESORT_HIST_TEX_SLOT) uniform sampler2D g_presort_hist_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_presort_img;
layout(binding = OUT2_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_downsampled_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

// Separation weight ramp: ramps from the tile's near extreme to the far side. Enscape anchored it on wsDepth-max
// with flipped d, because its linear depth is negative and decreasing (ours is positive and increasing).
vec2 depthComp2(float depth, float closestDepth, float midTile) {
    const float DOF_SCALE_FOREGROUND = 1.0 / midTile;
    const float d = DOF_SCALE_FOREGROUND * (depth - closestDepth);
    const float background = smoothstep(0.0, 1.0, d);
    return vec2(background, 1.0 - background);
}

// Maps a pattern offset onto the bokeh shape: base applies the anamorphic stretch only; @LENS_SHAPE evaluates
// the circumradius-1 polygon with lens_params.x blades (same ngon_rad() as Ray) oriented by lens_params.z.
vec2 ApertureShape(const vec2 offs) {
#ifdef LENS_SHAPE
    const float len = length(offs);
    if (len <= 0.0) {
        return offs;
    }
    const vec2 dir = offs / len;
    const float blades = g_params.lens_params.x;
    const float seg = 2.0 * M_PI / blades;
    // Ray's parametrization: aperture point at angle theta is (sin(theta), cos(theta)) rotated by the lens rotation
    const float theta = atan(dir.x, dir.y) - g_params.lens_params.z;
    const float radius = cos(0.5 * seg) / cos(theta - seg * floor((theta + 0.5 * seg) / seg));
    const vec2 shaped = dir * (len * radius);
    return vec2(shaped.x / g_params.lens_params.y, shaped.y);
#else
    return vec2(offs.x / g_params.lens_params.y, offs.y);
#endif
}

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const uint seed = get_pixel_seed(icoord, g_params.seed);
    // Hide the visible tiling due to separation plane
    const vec2 offset_tile = 2.0 * get_scrambled_2d_rand(g_random_seq, RAND_DIM_DOF, seed, 0u) - 1.0;

    const vec2 uv = (vec2(icoord) + 0.5) / vec2(g_params.img_size.xy);

    const vec2 tile_grid = vec2(textureSize(g_coc_tiles_tex, 0)) / vec2(g_params.img_size.zw);
    const ivec2 tile_coord = clamp(ivec2(((vec2(icoord) + offset_tile) * 2.0 + 0.5) * tile_grid / TILE_RES), ivec2(0),
                                   ivec2(textureSize(g_coc_tiles_tex, 0).xy) - 1);
    const vec3 tile_fetch = texelFetch(g_coc_tiles_tex, tile_coord, 0).xyz;

    const float this_depth = LinearizeDepth(textureLod(g_depth_tex, uv, 0.0).x, g_params.clip_info);
    const float coc = min(MAX_KERNEL_RADIUS, BlurRadiusFromDelta(this_depth, FOCAL_DEPTH, COC_FACTOR, FOCAL_LENGTH));

    float mid_tile = min(2.5, (tile_fetch.x - tile_fetch.z) * 0.5);
    // Uniform-depth tiles (pure sky at infinity focus) would divide by zero here and NaN-propagate through
    // every filter gather; guard the scale away from zero, keeping its sign (negative = intentional flip).
    mid_tile = mid_tile < 0.0 ? min(mid_tile, -0.001) : max(mid_tile, 0.001);

    const vec2 center_weights = depthComp2(this_depth, tile_fetch.z, mid_tile);
    const float coc_signed = this_depth < FOCAL_DEPTH ? coc : -coc;
    imageStore(g_presort_img, icoord, vec4(coc_signed, SampleAlpha(coc) * center_weights, 1.0));

    const vec4 center_color = textureLod(g_color_tex, uv, 0.0);
    vec4 foreground = vec4(center_color.xyz, 1.0);
    vec4 background = foreground;
    vec3 color_min = center_color.xyz, color_max = center_color.xyz;

    // This denominator should be 3.0 to cover the in-ring-holes.
    const float blur_radius = max(1.0, coc / NUM_DOF_RINGS);
    const mat2 rotation = mat2(g_params.rot.xy, g_params.rot.zw);

    // See slides 109, 110, 115
    for (int i = 0; i < 8; ++i) {
        const vec2 fetch_pos = uv + ApertureShape(rotation * g_spiral8_Pi3[i]) * blur_radius / vec2(g_params.img_size.zw);
        const vec3 color_read = textureLod(g_color_tex, fetch_pos, 0.0).xyz;
        color_min = min(color_min, color_read);
        color_max = max(color_max, color_read);
        const float sample_depth = LinearizeDepth(textureLod(g_depth_tex, fetch_pos, 0.0).x, g_params.clip_info);
        const vec2 sample_weights = depthComp2(sample_depth, tile_fetch.z, mid_tile);
        // The Karis-Average as mentioned in the CoD slides is not necessary due to the temporal stabilization
        foreground += sample_weights.y * vec4(color_read, 1.0);
        background += sample_weights.x * vec4(color_read, 1.0);
    }

    const vec4 assembled_blur = mix(foreground, background, center_weights.x);

    // Temporal accumulation of the halfres blur input (color + signed CoC): reprojection uses the dilated
    // velocity in ren_res texels (vel_texel; blit_tsr sign convention), the clamp rejects bad history.
    const float DOF_ACCUM_WEIGHT = 0.15;
    vec4 accum_val = vec4(assembled_blur.xyz / assembled_blur.w, coc_signed);
    const vec2 vel = textureLod(g_velocity_tex, uv, 0.0).xy;
    const vec4 hist = textureLod(g_presort_hist_tex, uv - vel * g_params.vel_texel, 0.0);
    const float coc_hist = clamp(hist.w, coc_signed - coc, coc_signed + coc);
    accum_val = vec4(mix(clamp(hist.xyz, color_min, color_max), accum_val.xyz, DOF_ACCUM_WEIGHT),
                     mix(coc_hist, coc_signed, DOF_ACCUM_WEIGHT));
    imageStore(g_downsampled_img, icoord, accum_val);
}
