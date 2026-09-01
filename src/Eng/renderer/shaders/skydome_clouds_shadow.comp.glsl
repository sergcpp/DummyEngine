#version 430 core

#include "_cs_common.glsl"

#define ENABLE_CLOUDS_CURL 0
#include "atmosphere_common.glsl"

#include "skydome_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params2 g_params;
};

layout(binding = WEATHER_TEX_SLOT) uniform sampler2D g_weather_tex;
layout(binding = CURL_TEX_SLOT) uniform sampler2D g_curl_tex;
layout(binding = NOISE3D_TEX_SLOT) uniform sampler3D g_noise3d_tex;

layout(binding = OUT_B0_IMG_SLOT, r16f) uniform image2D g_out_b0_img;
layout(binding = OUT_B1234_IMG_SLOT, rgba16f) uniform image2D g_out_b1234_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 ucoord = gl_GlobalInvocationID.xy;
    const vec2 norm_uvs = (vec2(4 * ucoord + g_params.sample_coord) + 0.5) / CLOUD_SHADOWMAP_RES;

    vec4 ray_origin_cs = vec4(-1.0 + 2.0 * norm_uvs, 1.0, 1.0);
    ray_origin_cs.xy = RadialWarp(ray_origin_cs.xy, 2.0);

    const vec3 ray_origin_ws = TransformFromClipSpace(g_shrd_data.cloud_sh_world_from_clip, ray_origin_cs);
    const vec3 ray_dir_ws = (g_shrd_data.sun_dir.y > -0.025) ? -g_shrd_data.sun_dir.xyz : -vec3(0.707, 0.707, 0.0);

    const vec4 clouds_intersection = CloudsIntersection(ray_origin_ws, ray_dir_ws);
    const float clouds_beg = clouds_intersection.z, clouds_end = clouds_intersection.x;
    const float clouds_depth = min(clouds_end - clouds_beg, MOMENTS_MAX_DIST);

    const uint CloudsSampleCount = 32;
    const float step_size = clouds_depth / float(CloudsSampleCount);

    float t = clouds_beg + 0.001;
    pow_moments4_t moments;
    moments.b0 = 0.0;
    moments.b1234 = vec4(0.0);

    for (uint i = 0; i < CloudsSampleCount; ++i) {
        const vec3 local_position = ray_origin_ws + t * ray_dir_ws;

        float local_height, height_fraction;
        vec3 up_vector;
        const float local_density = GetCloudsDensity(g_weather_tex, g_curl_tex, g_noise3d_tex, local_position, local_height, height_fraction, up_vector);
        if (local_density > 0.0) {
            const float local_absorbance = local_density * step_size;
            UpdateMoments(local_absorbance, t - clouds_beg, moments);
        }

        t += step_size;
    }

    imageStore(g_out_b0_img, ivec2(ucoord), vec4(moments.b0, 0.0, 0.0, 0.0));
    imageStore(g_out_b1234_img, ivec2(ucoord), moments.b1234);
}