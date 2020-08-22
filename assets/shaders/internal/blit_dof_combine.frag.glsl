#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
PERM @MSAA_4
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_original;
layout(binding = REN_BASE1_TEX_SLOT) uniform sampler2D s_small_blur;
layout(binding = REN_BASE2_TEX_SLOT) uniform sampler2D s_large_blur;
#if defined(MSAA_4)
layout(binding = 3) uniform mediump sampler2DMS s_depth;
#else
layout(binding = 3) uniform mediump sampler2D s_depth;
#endif
layout(binding = 4) uniform sampler2D s_coc;

layout(location = 0) uniform highp vec4 uTransform;
layout(location = 1) uniform vec3 uDofEquation;
layout(location = 2) uniform vec4 uDofLerpScale;
layout(location = 3) uniform vec4 uDofLerpBias;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

vec4 sampleWithOffset(sampler2D tex, vec2 uvs, vec2 offset_px) {
    return textureLod(tex, uvs + offset_px / uTransform.zw, 0.0);
}

vec3 GetSmallBlurSample(vec2 uvs) {
    const float weight = 4.0 / 17.0;
    vec3 sum = vec3(0.0);
    sum += weight * sampleWithOffset(s_original, uvs, vec2(+0.5, +1.5)).rgb;
    sum += weight * sampleWithOffset(s_original, uvs, vec2(-1.5, -0.5)).rgb;
    sum += weight * sampleWithOffset(s_original, uvs, vec2(-0.5, +1.5)).rgb;
    sum += weight * sampleWithOffset(s_original, uvs, vec2(+1.5, +0.5)).rgb;
    return sum;
}

vec4 InterpolateDof(vec3 small, vec3 med, vec3 large, float t) {
    vec4 weights = clamp(t * uDofLerpScale + uDofLerpBias, vec4(0.0), vec4(1.0));
    weights.yz = min(weights.yz, vec2(1.0) - weights.xy);

    vec3 color = weights.y * small + weights.z * med + weights.w * large;
    float alpha = dot(weights.yzw, vec3(16.0 / 17.0, 1.0, 1.0));

    return vec4(color, alpha);
}

void main() {
    vec2 norm_uvs = aVertexUVs_ / uTransform.zw;

    vec3 small = GetSmallBlurSample(norm_uvs);
    vec3 med = textureLod(s_small_blur, norm_uvs, 0.0).rgb;
    vec3 large = textureLod(s_large_blur, norm_uvs, 0.0).rgb;
    float near_coc = textureLod(s_coc, norm_uvs, 0.0).r;

    float depth = LinearizeDepth(texelFetch(s_depth, ivec2(aVertexUVs_), 0).x,
                                 shrd_data.uClipInfo);

    float coc;
    if (depth > 100000.0) {
        coc = near_coc;
    } else {
        float far_coc = clamp(uDofEquation.x * depth + uDofEquation.y, 0.0, 1.0);
        coc = max(near_coc, far_coc * uDofEquation.z);
    }

    vec4 col = InterpolateDof(small, med.rgb, large, coc);

    outColor.rgb = col.rgb + texelFetch(s_original, ivec2(aVertexUVs_), 0).rgb * (1.0 - col.a);
    outColor.a = 1.0;
}
