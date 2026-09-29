#version 430 core

#include "_cs_common.glsl"
#include "dof_interface.h"
#include "dof_common.glsl"
#include "pmj_common.glsl"

#pragma multi_compile _ LENS_SHAPE
// Only the @TONEMAP permutations are ever loaded (Renderer::pi_dof_filter_); plain variants are emitted by
// asset prep but unused. See MaybeTonemap below.
#pragma multi_compile _ TONEMAP

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DOWNSAMPLED_TEX_SLOT) uniform sampler2D g_downsampled_color_tex;
layout(binding = COC_TILES_TEX_SLOT) uniform sampler2D g_coc_tiles_tex;
layout(binding = PRESORT_TEX_SLOT) uniform sampler2D g_presort_tex;
layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_filtered_img;
layout(binding = OUT2_IMG_SLOT, r16f) uniform restrict writeonly image2D g_alpha_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

// Same as in blit_tsr.frag.glsl (https://gpuopen.com/optimized-reversible-tonemapper-for-resolve/): the bokeh
// gathering averages in the compressed range so HDR fireflies can not dominate it; the inverse is exact.
vec3 MaybeTonemap(vec3 c) {
    c = clamp(c, vec3(0.0), vec3(HALF_MAX));
#if defined(TONEMAP)
    c = c / (c + vec3(1.0));
#endif
    return c;
}

vec3 TonemapInvert(vec3 c) {
#if defined(TONEMAP)
    c = c / (vec3(1.0) - c);
#endif
    c = clamp(c, vec3(0.0), vec3(HALF_MAX));
    return c;
}

float SpreadToe(float offsetCoc, float spreadCmp) {
    // In the slides, an unknown toe factor is mentioned.
    const float TOE_FACTOR = 3.0;
    return offsetCoc <= 1.0 ? pow(spreadCmp, TOE_FACTOR) : spreadCmp;
}

float SpreadCmp(float offsetCoc, float sampleCoc) {
    // Usually, the addition should be + 1.0, it can be adjusted to catch false weights
    return SpreadToe(offsetCoc, saturate(sampleCoc - offsetCoc + 1.0));
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
    const vec2 uv = (vec2(icoord) + 0.5) / vec2(g_params.img_size.xy);

    // Hide the visible tiling due to separation plane
    const vec2 offset_tile = 2.0 * (get_scrambled_2d_rand(g_random_seq, RAND_DIM_DOF, seed, 1u) - 0.5) /
                             vec2(g_params.img_size.xy);
    const vec2 tile_clamp = vec2(textureSize(g_coc_tiles_tex, 0)) / vec2(textureSize(g_coc_tiles_tex, 0));
    const vec3 tile_fetch = textureLod(g_coc_tiles_tex, min(uv + offset_tile, tile_clamp), 0.0).xyz;
    const float max_coc_in_tile = tile_fetch.y;

    if (max_coc_in_tile < EARLY_OUT_MAX_RADIUS) {
        imageStore(g_filtered_img, icoord, vec4(texelFetch(g_downsampled_color_tex, icoord, 0).xyz, 1.0));
        imageStore(g_alpha_img, icoord, vec4(0.0));
        return;
    }

    vec4 background = vec4(0.0);
    vec4 foreground = vec4(0.0);
    const float pixel_to_sample_units_scale = NUM_DOF_RINGS / max_coc_in_tile;
    const mat2 scaled_rotation = max_coc_in_tile * mat2(g_params.rot.xy, g_params.rot.zw);

    for (int i = 0; i < 49; ++i) {
        const ivec2 sample_pos = icoord + ivec2(ApertureShape(scaled_rotation * g_dof_sampling_rings[i].xy));
        const bool inside = all(greaterThanEqual(sample_pos, ivec2(0))) &&
                            all(lessThan(sample_pos, ivec2(g_params.img_size.xy)));

        vec3 presort_fetch = vec3(0.0);
        vec4 sample_color = vec4(0.0);
        if (inside) {
            const ivec2 fetch_pos = clamp(sample_pos, ivec2(0), ivec2(textureSize(g_presort_tex, 0).xy) - 1);
            presort_fetch = texelFetch(g_presort_tex, fetch_pos, 0).xyz;
            sample_color = vec4(MaybeTonemap(texelFetch(g_downsampled_color_tex, fetch_pos, 0).xyz), 1.0);
        } else {
            // Do not count out of screen fragments
            continue;
        }

        presort_fetch.yz *= SpreadCmp(g_dof_sampling_rings[i].z, abs(presort_fetch.x) * pixel_to_sample_units_scale);

        background += presort_fetch.y * sample_color;
        // Only out-of-focus samples own a bokeh disc; SampleAlpha() floors weights at the 1-px-disc density,
        // so un-gated in-focus samples (e.g. sky) would swamp the foreground layer and hard-cut the edge.
        presort_fetch.z *= smoothstep(0.0, 1.0, abs(presort_fetch.x));
        // This hack is from Jimenez, slide 105. It creates severe artifacts but hides the original silhouette.
        // Should be replaced by proper background reconstruction as done in "Life of a bokeh".
        presort_fetch.z *= presort_fetch.x < 0.0 ? 1.0 : 2.0;
        foreground += presort_fetch.z * sample_color;
    }

    // Ensure a valid content in both buffers after normalization to allow for blending.
    // Else, we risk blending into darkness if one is just black.
    foreground += 1e-6 * background;
    background += 1e-6 * foreground;
    foreground.xyz /= foreground.a;
    background.xyz /= background.a;

    const float alpha_out = saturate(foreground.a / (SampleAlpha(max_coc_in_tile) * 49.0));
    imageStore(g_alpha_img, icoord, vec4(alpha_out));
    // The gathering averaged in the MaybeTonemap space, get back to scene-referred HDR
    imageStore(g_filtered_img, icoord, vec4(TonemapInvert(mix(background.xyz, foreground.xyz, alpha_out)), 1.0));
}
