#version 430 core

#include "_cs_common.glsl"
#include "skydome_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params2 g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;
layout(binding = SKY_TEX_SLOT) uniform sampler2D g_sky_tex;
layout(binding = SKY_HIST_TEX_SLOT) uniform sampler2D g_sky_hist_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform image2D g_out_img;
layout(binding = OUT_HIST_IMG_SLOT, rgba16f) uniform image2D g_out_hist_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    const vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    const vec3 ray_origin_ss = vec3(norm_uvs, 0.0);

    vec2 unjitter = g_shrd_data.taa_info.xy;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif

    const vec4 ray_origin_cs = vec4(2.0 * ray_origin_ss.xy - 1.0 - unjitter, ray_origin_ss.z, 1.0);
    const vec3 ray_origin_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, ray_origin_cs);

    const vec3 view_ray_ws = normalize(ray_origin_ws);

    // initial fallback color
    vec4 out_color = textureLod(g_env_tex, view_ray_ws, 0.0);
    // sun disk is missing from cubemap, add it here
    if (g_shrd_data.sun_dir.w > 0.0 && view_ray_ws.y > -0.01) {
        const float costh = dot(view_ray_ws, g_shrd_data.sun_dir.xyz);
        const float cos_theta = g_shrd_data.sun_col.w;
        const float sun_disk = smoothstep(cos_theta - SKY_SUN_BLEND_VAL, cos_theta + SKY_SUN_BLEND_VAL, costh);
        out_color.xyz += sun_disk * g_shrd_data.sun_col.xyz;
    }
    out_color.xyz = compress_hdr(out_color.xyz);

    vec4 prev_ray_origin_cs = g_shrd_data.prev_clip_from_world * vec4(ray_origin_ws, 1.0);
    prev_ray_origin_cs /= prev_ray_origin_cs.w;
#if defined(VULKAN)
    prev_ray_origin_cs.y = -prev_ray_origin_cs.y;
#endif

    bool no_history = true;
    const vec2 hist_uvs = 0.5 * prev_ray_origin_cs.xy + 0.5;
    if (all(greaterThan(hist_uvs, vec2(0.0))) && all(lessThan(hist_uvs, vec2(1.0)))) {
        const vec4 history = textureLod(g_sky_hist_tex, hist_uvs, 0.0);
        if (history.w > 0.0) {
            out_color = history;
            no_history = false;
        }
    }

    if (all(equal(icoord % 4, g_params.sample_coord)) || no_history) {
        const vec4 new_sample = texelFetch(g_sky_tex, icoord/4, 0);
        if (new_sample.w > 0.0) {
            out_color = new_sample;
        }
    }

    imageStore(g_out_hist_img, icoord, vec4(out_color.xyz, 1.0));
    const float depth = texelFetch(g_depth_tex, icoord, 0).r;
    if (depth == 0.0) {
        imageStore(g_out_img, icoord, vec4(out_color.xyz, 1.0));
    }
}
