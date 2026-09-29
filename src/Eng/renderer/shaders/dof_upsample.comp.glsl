#version 430 core

#include "_cs_common.glsl"
#include "dof_interface.h"
#include "dof_common.glsl"
#include "pmj_common.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = COLOR_TEX_SLOT) uniform sampler2D g_color_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = COC_TILES_TEX_SLOT) uniform sampler2D g_coc_tiles_tex;
layout(binding = FILTERED_TEX_SLOT) uniform sampler2D g_filtered_tex;
layout(binding = ALPHA_TEX_SLOT) uniform sampler2D g_alpha_tex;
layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

const vec2 PoissonDisk8[8] = vec2[](vec2(-0.4425296, -0.1756687), vec2(-0.1982351, 0.7117441),
                                    vec2(-0.8524727, 0.4356169), vec2(0.3295574, -0.5738078),
                                    vec2(0.2541587, 0.01586004), vec2(-0.263842, -0.8562326),
                                    vec2(0.8631233, -0.1870599), vec2(0.3779924, 0.5789691));

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const uint seed = get_pixel_seed(icoord, g_params.seed);
    const vec2 uv = (vec2(icoord) + 0.5) / vec2(g_params.img_size.xy);

    // Hide the visible tiling due to separation plane
    const vec2 offset_tile = get_scrambled_2d_rand(g_random_seq, DOF_RANDOM_DIM, seed, 1u) - 0.5;
    const ivec2 tile_coord = clamp((icoord + ivec2(offset_tile)) / int(TILE_RES), ivec2(0),
                                   ivec2(textureSize(g_coc_tiles_tex, 0).xy) - 1);
    const vec3 coc_fetch = texelFetch(g_coc_tiles_tex, tile_coord, 0).xyz;
    const float tile_max_coc = coc_fetch.y;
    const float tile_max_depth = coc_fetch.x;
    const float tile_depth_range = abs(coc_fetch.x - coc_fetch.z);

    const float this_depth = LinearizeDepth(texture(g_depth_tex, uv).x, g_params.clip_info);
    // Convert from halfRes units to fullres units
    const float full_res_coc =
        2.0 * min(MAX_KERNEL_RADIUS, blurRadiusFromDelta(this_depth, FOCAL_DEPTH, COC_FACTOR, FOCAL_LENGTH));

    const vec3 half_res = texture(g_filtered_tex, uv).rgb;
    const float alpha_upscaled = texture(g_alpha_tex, uv).x;

    float background_factor = linstep(4.0, 3.0, full_res_coc);
    float foreground_factor = linstep(3.0, 2.0, tile_max_coc);

    // Blend into fullres layer if we have a small enough center COC and are near the foreground plane
    foreground_factor = mix(1.0, foreground_factor,
                            saturate(full_res_coc * 0.5 + 3.0 * abs(tile_max_depth - this_depth) / (1e-9 + tile_depth_range)));
    float combined_factor = mix(background_factor, foreground_factor, alpha_upscaled);
    // When the halfRes maxRadius is small enough, show foreground
    combined_factor = mix(combined_factor, 1.0, saturate(2.5 - tile_max_coc));

    // Blur foreground a bit to ensure smooth transition at low radii (Slide 119)
    const vec3 center_read = texture(g_color_tex, uv).xyz;
    vec3 full_res = center_read;

    const int quad_id = (icoord.x % 2) + ((icoord.y / 2) % 2);
    const float MIN_COC_FULLRES = 0.2;
    if (combined_factor > 0.01 && tile_max_coc > MIN_COC_FULLRES && full_res_coc > 0.4) {
        const float full_res_coc_uv = min(2.0 * EARLY_OUT_MAX_RADIUS, full_res_coc) / float(g_params.img_size.x);
        const float angle = g_params.rot_angle + float(quad_id) * M_PI * 0.5;
        const float ca = cos(angle), sa = sin(angle);
        const mat2 rotation = mat2(ca, sa, -sa, ca);

        for (int i = 0; i < 8; ++i) {
            full_res += texture(g_color_tex, uv + full_res_coc_uv * (rotation * PoissonDisk8[i])).rgb;
        }
        // 9 because center fetch (1) + spiral (8)
        full_res /= 9.0;

        // Ensure a smooth gradient
        full_res = mix(center_read, full_res, linstep(MIN_COC_FULLRES, 0.7, tile_max_coc));
    }

    imageStore(g_out_img, icoord, vec4(mix(half_res, full_res, saturate(combined_factor)), 1.0));
}
