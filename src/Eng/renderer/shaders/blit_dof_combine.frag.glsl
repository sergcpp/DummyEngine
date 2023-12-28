#version 310 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
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
    SharedData g_shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D g_original;
layout(binding = REN_BASE1_TEX_SLOT) uniform sampler2D g_small_blur;
layout(binding = REN_BASE2_TEX_SLOT) uniform sampler2D g_large_blur;
#if defined(MSAA_4)
layout(binding = 3) uniform mediump sampler2DMS g_depth;
#else
layout(binding = 3) uniform mediump sampler2D g_depth;
#endif
layout(binding = 4) uniform sampler2D g_coc;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) highp vec4 g_ransform;
                        vec4 g_dof_lerp_scale;
                        vec4 g_dof_lerp_bias;
                        vec3 g_dof_equation;
};
#else
layout(location = 0) uniform highp vec4 g_ransform;
layout(location = 2) uniform vec4 g_dof_lerp_scale;
layout(location = 3) uniform vec4 g_dof_lerp_bias;
layout(location = 1) uniform vec3 g_dof_equation;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

vec4 sampleWithOffset(sampler2D tex, vec2 uvs, vec2 offset_px) {
    return textureLod(tex, uvs + offset_px / g_ransform.zw, 0.0);
}

vec3 GetSmallBlurSample(vec2 uvs) {
    const float weight = 4.0 / 17.0;
    vec3 sum = vec3(0.0);
    sum += weight * sampleWithOffset(g_original, uvs, vec2(+0.5, +1.5)).rgb;
    sum += weight * sampleWithOffset(g_original, uvs, vec2(-1.5, -0.5)).rgb;
    sum += weight * sampleWithOffset(g_original, uvs, vec2(-0.5, +1.5)).rgb;
    sum += weight * sampleWithOffset(g_original, uvs, vec2(+1.5, +0.5)).rgb;
    return sum;
}

vec4 InterpolateDof(vec3 small, vec3 med, vec3 large, float t) {
    vec4 weights = clamp(t * g_dof_lerp_scale + g_dof_lerp_bias, vec4(0.0), vec4(1.0));
    weights.yz = min(weights.yz, vec2(1.0) - weights.xy);

    vec3 color = weights.y * small + weights.z * med + weights.w * large;
    float alpha = dot(weights.yzw, vec3(16.0 / 17.0, 1.0, 1.0));

    return vec4(color, alpha);
}

void main() {
    vec2 norm_uvs = g_vtx_uvs / g_ransform.zw;

    vec3 small = GetSmallBlurSample(norm_uvs);
    vec3 med = textureLod(g_small_blur, norm_uvs, 0.0).rgb;
    vec3 large = textureLod(g_large_blur, norm_uvs, 0.0).rgb;
    float near_coc = textureLod(g_coc, norm_uvs, 0.0).r;

    float depth = LinearizeDepth(texelFetch(g_depth, ivec2(g_vtx_uvs), 0).x, g_shrd_data.clip_info);

    float coc;
    if (depth > 100000.0) {
        coc = near_coc;
    } else {
        float far_coc = clamp(g_dof_equation.x * depth + g_dof_equation.y, 0.0, 1.0);
        coc = max(near_coc, far_coc * g_dof_equation.z);
    }

    vec4 col = InterpolateDof(small, med.rgb, large, coc);

    g_out_color.rgb = col.rgb + texelFetch(g_original, ivec2(g_vtx_uvs), 0).rgb * (1.0 - col.a);
    g_out_color.a = 1.0;
}
