#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#pragma multi_compile _ SCREEN

#include "_fs_common.glsl"
#include "skydome_interface.h"

#ifdef SCREEN
    #define ENABLE_SUN_DISK 1
#else
    #define ENABLE_SUN_DISK 0
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
layout(binding = NOISE3D_TEX_SLOT) uniform sampler3D g_noise3d_tex;

layout(location = 0) in highp vec3 g_vtx_pos;

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

    const vec3 rotated_dir = rotate_xz(view_dir_ws, g_shrd_data.env_col.w);
    const uint rand_hash = superfast(uvec3(g_vtx_pos * 100.0));
    g_out_color.rgb = g_shrd_data.env_col.xyz * IntegrateScattering(vec3(0.0, g_shrd_data.atmosphere.viewpoint_height, 0.0), view_dir_ws, FLT_MAX, rand_hash,
                                                                    g_trasmittance_lut, g_multiscatter_lut, g_moon_tex, g_weather_tex, g_cirrus_tex, g_noise3d_tex);
    g_out_color.a = 1.0;
}
