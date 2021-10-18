#version 310 es

#include "_fs_common.glsl"
#include "blit_bilateral_interface.glsl"

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

layout(binding = DEPTH_TEX_SLOT) uniform mediump sampler2D depth_texture;
layout(binding = INPUT_TEX_SLOT) uniform lowp sampler2D input_texture;

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

LAYOUT(location = 0) in highp vec2 aVertexUVs_;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 norm_uvs = aVertexUVs_;
    vec2 texel_size = vec2(1.0) / params.resolution;

    float center_depth = textureLod(depth_texture, norm_uvs, 0.0).r;
    float closeness = 1.0 / (0.075 + 0.0);
    float weight = closeness * 0.214607;
    outColor = vec4(0.0);
    outColor += textureLod(input_texture, norm_uvs, 0.0) * weight;

    float normalization = weight;

    if(params.vertical < 0.5) {
        float depth;

        // 0.071303 0.131514 0.189879 0.214607

        depth = textureLod(depth_texture, norm_uvs - vec2(texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        outColor += textureLod(input_texture, norm_uvs - vec2(texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs + vec2(texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        outColor += textureLod(input_texture, norm_uvs + vec2(texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs - vec2(2.0 * texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        outColor += textureLod(input_texture, norm_uvs - vec2(2.0 * texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs + vec2(2.0 * texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        outColor += textureLod(input_texture, norm_uvs + vec2(2.0 * texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs - vec2(3.0 * texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        outColor += textureLod(input_texture, norm_uvs - vec2(3.0 * texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs + vec2(3.0 * texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        outColor += textureLod(input_texture, norm_uvs + vec2(3.0 * texel_size.x, 0), 0) * weight;
        normalization += weight;
    } else {
        float depth;

        depth = textureLod(depth_texture, norm_uvs - vec2(0, texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        outColor += textureLod(input_texture, norm_uvs - vec2(0, texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs + vec2(0, texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        outColor += textureLod(input_texture, norm_uvs + vec2(0, texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs - vec2(0, 2.0 * texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        outColor += textureLod(input_texture, norm_uvs - vec2(0, 2.0 * texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs + vec2(0, 2.0 * texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        outColor += textureLod(input_texture, norm_uvs + vec2(0, 2.0 * texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs - vec2(0, 3.0 * texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        outColor += textureLod(input_texture, norm_uvs - vec2(0, 3.0 * texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(depth_texture, norm_uvs + vec2(0, 3.0 * texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        outColor += textureLod(input_texture, norm_uvs + vec2(0, 3.0 * texel_size.y), 0) * weight;
        normalization += weight;
    }

    outColor /= normalization;
}

