#version 320 es
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "_texturing.glsl"

#pragma multi_compile _ OUTPUT_VELOCITY
#pragma multi_compile _ TRANSPARENT
#pragma multi_compile _ HASHED_TRANSPARENCY

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#ifdef TRANSPARENT
    #if !defined(BINDLESS_TEXTURES)
        layout(binding = BIND_MAT_TEX4) uniform sampler2D g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT

#ifdef OUTPUT_VELOCITY
    layout(location = 0) in highp vec3 g_vtx_pos_cs_curr;
    layout(location = 1) in highp vec3 g_vtx_pos_cs_prev;
#endif // OUTPUT_VELOCITY
#ifdef TRANSPARENT
    layout(location = 2) in highp vec2 g_vtx_uvs0;
    #ifdef HASHED_TRANSPARENCY
        layout(location = 3) in highp vec3 g_vtx_pos_ls;
    #endif // HASHED_TRANSPARENCY
    #if defined(BINDLESS_TEXTURES)
        layout(location = 4) in flat highp TEX_HANDLE g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT

#ifdef OUTPUT_VELOCITY
layout(location = 0) out vec2 g_out_velocity;
#endif

void main() {
#ifdef TRANSPARENT
    float tx_alpha = texture(SAMPLER2D(g_alpha_tex), g_vtx_uvs0).r;
#ifndef HASHED_TRANSPARENCY
    if (tx_alpha < 0.9) discard;
#else // HASHED_TRANSPARENCY
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
#endif // HASHED_TRANSPARENCY
#endif // TRANSPARENT

#ifdef OUTPUT_VELOCITY
    highp vec2 curr = g_vtx_pos_cs_curr.xy / g_vtx_pos_cs_curr.z;
    highp vec2 prev = g_vtx_pos_cs_prev.xy / g_vtx_pos_cs_prev.z;
    g_out_velocity = 0.5 * (curr + g_shrd_data.taa_info.xy - prev);
#endif // OUTPUT_VELOCITY
}

