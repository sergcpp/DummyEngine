#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = 0) uniform mediump sampler2D s_color_curr;
layout(binding = 1) uniform mediump sampler2D s_color_hist;

layout(binding = 2) uniform mediump sampler2D s_depth;
layout(binding = 3) uniform mediump sampler2D s_velocity;

layout(location = 13) uniform vec2 uTexSize;
layout(location = 14) uniform float uExposure;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outHistory;

// http://twvideo01.ubm-us.net/o1/vault/gdc2016/Presentations/Pedersen_LasseJonFuglsang_TemporalReprojectionAntiAliasing.pdf
vec3 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) {
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min);
    vec3 v_clip = q - p_clip;
    vec3 v_unit = v_clip / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));
    if (ma_unit > 1.0) {
        return p_clip + v_clip / ma_unit;
    } else {
        return q;
    }
}

// https://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
float max3(float x, float y, float z) { return max(x, max(y, z)); }
float rcp(float x) { return 1.0 / x; }

vec3 Tonemap(in vec3 c) {
    c *= uExposure;
    return c * rcp(max3(c.r, c.g, c.b) + 1.0);
}

vec3 TonemapInvert(in vec3 c) {
    return (1.0 / uExposure) * c * rcp(1.0 - max3(c.r, c.g, c.b));
}

float luma(vec3 col) {
    return dot(col, vec3(0.2125, 0.7154, 0.0721));
}

void main() {
    ivec2 uvs_px = ivec2(aVertexUVs_);
    vec2 norm_uvs = aVertexUVs_ / uTexSize;

    vec3 col_curr = Tonemap(texelFetch(s_color_curr, uvs_px, 0).rgb);

    float min_depth = texelFetch(s_depth, uvs_px, 0).r;

    const ivec2 offsets[8] = ivec2[8](
        ivec2(-1, 1),   ivec2(0, 1),    ivec2(1, 1),
        ivec2(-1, 0),                   ivec2(1, 0),
        ivec2(-1, -1),  ivec2(1, -1),   ivec2(1, -1)
    );

    vec3 col_avg = col_curr, col_var = col_curr * col_curr;
    ivec2 closest_frag = ivec2(0, 0);

    for (int i = 0; i < 8; i++) {
        float depth = texelFetch(s_depth, uvs_px + offsets[i], 0).r;
        if (depth < min_depth) {
            closest_frag = offsets[i];
            min_depth = depth;
        }

        vec3 col = Tonemap(texelFetch(s_color_curr, uvs_px + offsets[i], 0).rgb);
        col_avg += col;
        col_var += col * col;
    }

    col_avg /= 9.0;
    col_var /= 9.0;

    vec3 sigma = sqrt(max(col_var - col_avg * col_avg, vec3(0.0)));
    vec3 col_min = col_avg - 1.25 * sigma;
    vec3 col_max = col_avg + 1.25 * sigma;

    vec2 vel = texelFetch(s_velocity, uvs_px + closest_frag, 0).rg;
    vec3 col_hist = Tonemap(textureLod(s_color_hist, norm_uvs - vel, 0.0).rgb);

    //col_hist = clip_aabb(col_min, col_max, col_curr, col_hist);
    col_hist = clamp(col_hist, col_min, col_max);

    float weight = 0.04;
    //float weight = 1.0 - 1.0 / (1.0 + luma(col_curr));
    vec3 col = mix(col_hist, col_curr, weight);
    outColor = TonemapInvert(col);
    outHistory = outColor;
}
