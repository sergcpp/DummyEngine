#version 430 core
#extension GL_ARB_texture_multisample : enable

#include "_fs_common.glsl"
#include "blit_ssr_interface.h"

#define Z_THICKNESS 0.05
#define STRIDE 0.0125
#define MAX_TRACE_STEPS 48
#define BSEARCH_STEPS 4

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 out_color;

float LinearDepthFetch_Nearest(vec2 hit_uv) {
    //return texelFetch(g_depth_tex, hit_pixel / 2, 0).r;
    return 0.0;
}

float LinearDepthFetch_Bilinear(vec2 hit_uv) {
    return 0.0;
}

#include "ss_trace_simple.glsl.inl"

void main() {
    out_color = vec4(0.0);

    ivec2 pix_uvs = ivec2(g_vtx_uvs + vec2(0.5));
    vec2 norm_uvs = 2.0 * g_vtx_uvs / g_shrd_data.res_and_fres.xy;

    vec4 normal_fetch = texelFetch(g_norm_tex, 2 * pix_uvs, 0);
    vec4 normal_roughness = UnpackNormalAndRoughness(normal_fetch);
    if (normal_roughness.w > 0.95) return;

    float depth = DelinearizeDepth(texelFetch(g_depth_tex, pix_uvs, 0).r, g_shrd_data.clip_info);

    vec3 normal_ws = normal_roughness.xyz;
    vec3 normal_vs = (g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz;

    const vec4 ray_origin_cs = vec4(2.0 * norm_uvs - 1.0, depth, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);

    vec3 view_ray_vs = normalize(ray_origin_vs);
    vec3 refl_ray_vs = reflect(view_ray_vs, normal_vs);

    ivec2 c = pix_uvs;
    float jitter = float((c.x + c.y) & 1) * 0.5;

    vec2 hit_uv;
    vec3 hit_point;

    if (IntersectRay(ray_origin_vs, refl_ray_vs, jitter, hit_uv, hit_point)) {
        // reproject hitpoint into a clip space of previous frame
        vec4 hit_prev = g_shrd_data.delta_matrix * vec4(hit_point, 1.0);
#if defined(VULKAN)
        hit_prev.y = -hit_prev.y;
#endif // VULKAN
        hit_prev /= hit_prev.w;
        hit_prev.xy = 0.5 * hit_prev.xy + 0.5;

        float mm = max(abs(0.5 - hit_prev.x), abs(0.5 - hit_prev.y));
        float mix_factor = min(4.0 * (1.0 - 2.0 * mm), 1.0);

        out_color.rg = hit_prev.xy;
        out_color.b = mix_factor;
    }
}

