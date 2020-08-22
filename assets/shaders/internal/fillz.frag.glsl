#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

/*
PERM @TRANSPARENT_PERM
PERM @OUTPUT_VELOCITY
PERM @TRANSPARENT_PERM;OUTPUT_VELOCITY
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

#ifdef TRANSPARENT_PERM
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D alphatest_texture;
#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs1_;
#else
in vec2 aVertexUVs1_;
#endif

#ifdef HASHED_TRANSPARENCY
layout(location = 3) uniform float hash_scale;
#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 1) in vec3 aVertexObjCoord_;
#else
in vec3 aVertexObjCoord_;
#endif
#endif
#endif

#ifdef OUTPUT_VELOCITY
#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 2) in vec3 aVertexCSCurr_;
layout(location = 3) in vec3 aVertexCSPrev_;
#else
in vec3 aVertexCSCurr_;
in vec3 aVertexCSPrev_;
#endif

layout(location = 0) out vec2 outVelocity;
#endif

float hash(vec2 v) {
    return fract(1.0e4 * sin(17.0 * v.x + 0.1 * v.y) *
                 (0.1 + abs(sin(13.0 * v.y + v.x))));
}

float hash3D(vec3 v) {
    return hash(vec2(hash(v.xy), v.z));
}

void main() {
#ifdef TRANSPARENT_PERM
    float tx_alpha = texture(alphatest_texture, aVertexUVs1_).a;
#ifndef HASHED_TRANSPARENCY
    if (tx_alpha < 0.9) discard;
#else
    float max_deriv = max(length(dFdx(aVertexObjCoord_)),
                          length(dFdy(aVertexObjCoord_)));
    float pix_scale = 1.0 / (hash_scale * max_deriv);

    vec2 pix_scales = vec2(exp2(floor(log2(pix_scale))),
                           exp2(ceil(log2(pix_scale))));

    vec2 alpha = vec2(hash3D(floor(pix_scales.x * aVertexObjCoord_)),
                      hash3D(floor(pix_scales.y * aVertexObjCoord_)));

    float lerp_factor = fract(log2(pix_scale));

    float x = (1.0 - lerp_factor) * alpha.x + lerp_factor * alpha.y;

    float a = min(lerp_factor, 1.0 - lerp_factor);

    vec3 cases = vec3(x * x / (2.0 * a * (1.0 - a)),
                       (x - 0.5 * a) / (1.0 - a),
                       1.0 - ((1.0 - x) * (1.0 - x) / (2.0 * a * (1.0 - a))));

    float comp_a = (x < (1.0 - a)) ? ((x < a) ? cases.x : cases.y) : cases.z;
    comp_a = clamp(comp_a, 1.0e-6, 1.0);

    if (tx_alpha < comp_a) discard;
#endif
#endif

#ifdef OUTPUT_VELOCITY
    vec2 curr = aVertexCSCurr_.xy / aVertexCSCurr_.z;
    vec2 prev = aVertexCSPrev_.xy / aVertexCSPrev_.z;
    outVelocity = 0.5 * (curr + shrd_data.uTaaInfo.xy - prev);
#endif
}

