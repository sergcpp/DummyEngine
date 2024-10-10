#version 430 core

#pragma multi_compile _ SCREEN
#pragma multi_compile _ SUBSAMPLE

#if !defined(SCREEN) && defined(SUBSAMPLE)
#pragma dont_compile
#endif

#include "_fs_common.glsl"
#include "skydome_interface.h"

#ifdef SCREEN
    #define ENABLE_SUN_DISK 1
#else
    #define ENABLE_SUN_DISK 0
    #define ENABLE_CLOUDS_CURL 0
#endif

#include "atmosphere_common.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = TRANSMITTANCE_LUT_SLOT) uniform sampler2D g_trasmittance_lut;
layout(binding = MULTISCATTER_LUT_SLOT) uniform sampler2D g_multiscatter_lut;

layout(binding = MOON_TEX_SLOT) uniform sampler2D g_moon_tex;
layout(binding = WEATHER_TEX_SLOT) uniform sampler2D g_weather_tex;
layout(binding = CIRRUS_TEX_SLOT) uniform sampler2D g_cirrus_tex;
layout(binding = CURL_TEX_SLOT) uniform sampler2D g_curl_tex;
layout(binding = NOISE3D_TEX_SLOT) uniform sampler3D g_noise3d_tex;

#ifdef SUBSAMPLE
    layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
#endif

layout(location = 0) in vec3 g_vtx_pos;

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;

// SuperFastHash, adapated from http://www.azillionmonkeys.com/qed/hash.html
uint superfast(uvec3 data) {
    uint hash = 8u, tmp;

    hash += data.x & 0xffffu;
    tmp = (((data.x >> 16) & 0xffffu) << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    hash += hash >> 11;

    hash += data.y & 0xffffu;
    tmp = (((data.y >> 16) & 0xffffu) << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    hash += hash >> 11;

    hash += data.z & 0xffffu;
    tmp = (((data.z >> 16) & 0xffffu) << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    hash += hash >> 11;

    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

void main() {
    vec3 view_dir_ws = normalize(g_vtx_pos);
#if defined(VULKAN) && !defined(SCREEN)
    // ???
    view_dir_ws.z = -view_dir_ws.z;
#endif

#ifdef SUBSAMPLE
    ivec2 icoord = 4 * ivec2(gl_FragCoord.xy) + ivec2(g_params.sample_coord.x, g_params.sample_coord.y);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        g_out_color = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    const float depth_fetch = texelFetch(g_depth_tex, icoord, 0).x;
    /*if (depth_fetch != 0.0) {
        g_out_color = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }*/
    const vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;
    const vec4 pos_cs = vec4(2.0 * norm_uvs - 1.0, 0.0, 1.0);
    view_dir_ws = normalize(TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs));
#endif

    const vec3 rotated_dir = rotate_xz(view_dir_ws, g_shrd_data.env_col.w);
    //const uint rand_hash = superfast(uvec3(g_vtx_pos * 100.0));
    const uint rand_hash = superfast(uvec3(view_dir_ws * 5000.0 * 100.0));

    vec3 transmittance;
    vec3 radiance = g_shrd_data.env_col.xyz * IntegrateScattering(vec3(0.0, g_shrd_data.atmosphere.viewpoint_height, 0.0), view_dir_ws, FLT_MAX, rand_hash,
                                                                  g_trasmittance_lut, g_multiscatter_lut, g_moon_tex, g_weather_tex,
                                                                  g_cirrus_tex, g_curl_tex, g_noise3d_tex, transmittance);
#if defined(SCREEN)
    radiance = compress_hdr(radiance);
#endif
    g_out_color = vec4(radiance, 1.0);
}
