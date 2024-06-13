#version 320 es
#if !defined(VULKAN) && !defined(NO_BINDLESS) && defined(TRANSPARENT)
#extension GL_ARB_bindless_texture : enable
#endif

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "_texturing.glsl"

#pragma multi_compile _ OUTPUT_VELOCITY
#pragma multi_compile _ TRANSPARENT
#pragma multi_compile _ NO_BINDLESS

#if defined(NO_BINDLESS) && defined(VULKAN)
    #pragma dont_compile
#endif
#if defined(NO_BINDLESS) && !defined(TRANSPARENT)
    #pragma dont_compile
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#if defined(TRANSPARENT) && defined(NO_BINDLESS)
    layout(binding = BIND_MAT_TEX4) uniform sampler2D g_alpha_tex;
#endif // TRANSPARENT

#ifdef OUTPUT_VELOCITY
    layout(location = 0) in vec3 g_vtx_pos_cs_curr;
    layout(location = 1) in vec3 g_vtx_pos_cs_prev;
    layout(location = 2) in vec2 g_vtx_z_vs_curr;
    layout(location = 3) in vec2 g_vtx_z_vs_prev;
#endif // OUTPUT_VELOCITY
#ifdef TRANSPARENT
    layout(location = 4) in highp vec2 g_vtx_uvs0;
    layout(location = 5) in highp vec3 g_vtx_pos_ls;
    #if !defined(NO_BINDLESS)
        layout(location = 6) in flat highp TEX_HANDLE g_alpha_tex;
    #endif // !NO_BINDLESS
#endif // TRANSPARENT

#ifdef OUTPUT_VELOCITY
layout(location = 0) out vec4 g_out_velocity;
#endif

void main() {
#ifdef TRANSPARENT
    float tx_alpha = texture(SAMPLER2D(g_alpha_tex), g_vtx_uvs0).r;
    float max_deriv = max(length(dFdx(g_vtx_pos_ls)), length(dFdy(g_vtx_pos_ls)));
    const float HashScale = 0.1;
    float pix_scale = 1.0 / (HashScale * max_deriv);

    vec2 pix_scales = vec2(exp2(floor(log2(pix_scale))),
                           exp2(ceil(log2(pix_scale))));

    vec2 alpha = vec2(hash3D(floor(pix_scales.x * g_vtx_pos_ls)),
                      hash3D(floor(pix_scales.y * g_vtx_pos_ls)));

    float lerp_factor = fract(log2(pix_scale));

    float x = mix(alpha.x, alpha.y, lerp_factor);
    float a = min(lerp_factor, 1.0 - lerp_factor);

    vec3 cases = vec3(x * x / (2.0 * a * (1.0 - a)),
                       (x - 0.5 * a) / (1.0 - a),
                       1.0 - ((1.0 - x) * (1.0 - x) / (2.0 * a * (1.0 - a))));

    float comp_a = (x < (1.0 - a)) ? ((x < a) ? cases.x : cases.y) : cases.z;
    comp_a = clamp(comp_a, 1.0e-6, 1.0);

    if (tx_alpha < comp_a) discard;
#endif // TRANSPARENT

#ifdef OUTPUT_VELOCITY
    vec2 unjitter = g_shrd_data.taa_info.xy;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif
    const vec2 curr = g_vtx_pos_cs_curr.xy / g_vtx_pos_cs_curr.z;
    const vec2 prev = g_vtx_pos_cs_prev.xy / g_vtx_pos_cs_prev.z;
    g_out_velocity.xy = 0.5 * (curr + unjitter - prev);
#if defined(VULKAN)
    g_out_velocity.y = -g_out_velocity.y;
    g_out_velocity.xy *= g_shrd_data.res_and_fres.xy; // improve fp16 utilization
    const float curr_z = g_vtx_z_vs_curr.x / g_vtx_z_vs_curr.y;
    const float prev_z = g_vtx_z_vs_prev.x / g_vtx_z_vs_prev.y;
    g_out_velocity.z = -(curr_z - prev_z);
    g_out_velocity.w = 0.0;
#endif
#endif // OUTPUT_VELOCITY
}

