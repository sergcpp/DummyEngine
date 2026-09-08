#version 430 core

#pragma multi_compile _ NIGHT_TIME
#pragma multi_compile _ SCREEN
#pragma multi_compile _ SUBSAMPLE

#if !defined(SCREEN) && defined(SUBSAMPLE)
#pragma dont_compile
#endif

#include "_fs_common.glsl"
#include "skydome_interface.h"

#ifdef NIGHT_TIME
    #define IS_DAY_TIME 0
#else
    #define IS_DAY_TIME 1
#endif
#ifdef SCREEN
    #define ENABLE_RAY_JITTER 1
    #define ENABLE_SUN_DISK 1
    #define ENABLE_SHADOW_RAY 1
#else
    #define ENABLE_RAY_JITTER 0
    #define ENABLE_SUN_DISK 0
    #define ENABLE_SHADOW_RAY 0
#endif

#include "atmosphere_common.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = TCBN_1D_TEX_SLOT) uniform sampler2DArray g_tcbn_1d_tex;

layout(binding = TRANSMITTANCE_LUT_SLOT) uniform sampler2D g_trasmittance_lut;
layout(binding = MULTISCATTER_LUT_SLOT) uniform sampler2D g_multiscatter_lut;

layout(binding = MOON_TEX_SLOT) uniform sampler2D g_moon_tex;
layout(binding = WEATHER_TEX_SLOT) uniform sampler2D g_weather_tex;
layout(binding = CIRRUS_TEX_SLOT) uniform sampler2D g_cirrus_tex;
layout(binding = CURL_TEX_SLOT) uniform sampler2D g_curl_tex;
layout(binding = NOISE3D_TEX_SLOT) uniform sampler3D g_noise3d_tex;

layout(binding = MOMENTS_B0_TEX_SLOT) uniform sampler2D g_moments_b0_tex;
layout(binding = MOMENTS_B1234_TEX_SLOT) uniform sampler2D g_moments_b1234_tex;

#ifdef SUBSAMPLE
    layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
#endif

layout(location = 0) in vec3 g_vtx_pos;

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;

void main() {
    vec3 view_dir_ws = normalize(g_vtx_pos);
#if defined(VULKAN) && !defined(SCREEN)
    // ???
    view_dir_ws.z = -view_dir_ws.z;
#endif

#ifdef SUBSAMPLE
    const uvec3 ucoord = uvec3(4 * uvec2(gl_FragCoord.xy) + g_params.sample_coord.xy, 0 /* no per-frame jitter */);
    if (ucoord.x >= g_params.img_size.x || ucoord.y >= g_params.img_size.y) {
        g_out_color = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    const float depth_fetch = texelFetch(g_depth_tex, ivec2(ucoord.xy), 0).x;
    /*if (depth_fetch != 0.0) {
        g_out_color = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }*/
    const vec2 norm_uvs = (vec2(ucoord.xy) + 0.5) * g_shrd_data.fren_res.zw;
    const vec4 pos_cs = vec4(2.0 * norm_uvs - 1.0, 0.0, 1.0);
    view_dir_ws = normalize(TransformFromClipSpace(g_shrd_data.world_from_clip_no_translation, pos_cs));
#else
    const uvec3 ucoord = uvec3(gl_FragCoord.xy, g_params.frame_index);
#endif

    vec3 transmittance;
    vec3 radiance = g_shrd_data.env_col.xyz * IntegrateScattering(ucoord, vec3(0.0, g_shrd_data.atmosphere.viewpoint_height, 0.0), view_dir_ws, FLT_MAX,
                                                                  g_trasmittance_lut, g_multiscatter_lut, g_moon_tex, g_weather_tex,
                                                                  g_cirrus_tex, g_curl_tex, g_noise3d_tex, g_tcbn_1d_tex, g_moments_b0_tex, g_moments_b1234_tex, transmittance);
#if defined(SCREEN)
    radiance = compress_hdr(radiance, g_shrd_data.cam_pos_and_exp.w);
#endif
    g_out_color = vec4(radiance, 1.0);
}
