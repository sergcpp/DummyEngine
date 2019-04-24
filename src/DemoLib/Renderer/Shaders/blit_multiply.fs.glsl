R"(
#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/
        
)" __ADDITIONAL_DEFINES_STR__ R"(

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes;
};

layout(binding = 0) uniform sampler2D s_texture;
#if defined(MSAA_4)
layout(binding = 1) uniform mediump sampler2DMS s_mul_texture;
layout(binding = 2) uniform mediump sampler2DMS s_depth_texture;
layout(binding = 3) uniform mediump sampler2DMS s_norm_texture;
#else
layout(binding = 1) uniform mediump sampler2D s_mul_texture;
layout(binding = 2) uniform mediump sampler2DMS s_depth_texture;
layout(binding = 3) uniform mediump sampler2D s_norm_texture;
#endif

in vec2 aVertexUVs_;

out vec4 outColor;

float LinearDepthTexelFetch(ivec2 hit_pixel) {
    float depth = texelFetch(s_depth_texture, hit_pixel, 0).r;
    return uClipInfo[0] / (depth * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
}

vec3 DecodeNormal(vec2 enc) {
    vec4 nn = vec4(2.0 * enc, 0.0, 0.0) + vec4(-1.0, -1.0, 1.0, -1.0);
    float l = dot(nn.xyz, -nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(max(l, 0.0));
    return 2.0 * nn.xyz + vec3(0.0, 0.0, -1.0);
}

void main() {
    ivec2 pix_uvs = ivec2(aVertexUVs_ / 2.0) * 2;

    const ivec2 offsets[] = ivec2[4](
        ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1)
    );

    float norm_weights[4];
    vec3 normal = DecodeNormal(texelFetch(s_norm_texture, ivec2(aVertexUVs_), 0).xy);
    for (int i = 0; i < 4; i++) {
        vec3 norm_coarse = DecodeNormal(texelFetch(s_norm_texture, pix_uvs + 2 * offsets[i], 0).xy);
        norm_weights[i] = 0.0 + step(0.9, dot(norm_coarse, normal));
    }

    float depth_weights[4];
    float depth = LinearDepthTexelFetch(ivec2(aVertexUVs_));
    for (int i = 0; i < 4; i++) {
        float depth_coarse = LinearDepthTexelFetch(pix_uvs + 2 * offsets[i]);
        depth_weights[i] = 1.0 / (0.01 + abs(depth - depth_coarse));
    }

    vec2 sample_coord = fract(aVertexUVs_ / 2.0);

    float sample_weights[4];
    sample_weights[0] = (1.0 - sample_coord.x) * (1.0 - sample_coord.y);
    sample_weights[1] = sample_coord.x * (1.0 - sample_coord.y);
    sample_weights[2] = (1.0 - sample_coord.x) * sample_coord.y;
    sample_weights[3] = sample_coord.x * sample_coord.y;

    vec3 c0 = vec3(0.0);
    float weight_sum = 0.0;
    for (int i = 0; i < 4; i++) {
        float weight = sample_weights[i] * norm_weights[i] * depth_weights[i];
        c0 += texelFetch(s_texture, ivec2(aVertexUVs_)/2 + offsets[i], 0).xyz * weight;
        weight_sum += weight;
    }

    if (weight_sum > 0.0001) {
        c0 /= weight_sum;
    }

#if defined(MSAA_4)
    vec3 c1 = 0.25 * (texelFetch(s_mul_texture, ivec2(aVertexUVs_), 0).xyz +
                      texelFetch(s_mul_texture, ivec2(aVertexUVs_), 1).xyz +
                      texelFetch(s_mul_texture, ivec2(aVertexUVs_), 2).xyz +
                      texelFetch(s_mul_texture, ivec2(aVertexUVs_), 3).xyz);
#else
    vec3 c1 = texelFetch(s_mul_texture, ivec2(aVertexUVs_), 0).xyz;
#endif
            
    c0 *= c1;

    outColor = vec4(c0, 1.0);
}
)"