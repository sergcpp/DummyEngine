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

layout(binding = DEPTH_TEX_SLOT) uniform mediump sampler2D g_depth_tex;
layout(binding = INPUT_TEX_SLOT) uniform lowp sampler2D g_input_texture;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in highp vec2 g_vtx_uvs;
layout(location = 0) out vec4 g_out_color;

void main() {
    vec2 norm_uvs = g_vtx_uvs;
    vec2 texel_size = vec2(1.0) / g_params.resolution;

    float center_depth = textureLod(g_depth_tex, norm_uvs, 0.0).r;
    float closeness = 1.0 / (0.075 + 0.0);
    float weight = closeness * 0.214607;
    g_out_color = vec4(0.0);
    g_out_color += textureLod(g_input_texture, norm_uvs, 0.0) * weight;

    float normalization = weight;

    if(g_params.vertical < 0.5) {
        float depth;

        // 0.071303 0.131514 0.189879 0.214607

        depth = textureLod(g_depth_tex, norm_uvs - vec2(texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        g_out_color += textureLod(g_input_texture, norm_uvs - vec2(texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs + vec2(texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        g_out_color += textureLod(g_input_texture, norm_uvs + vec2(texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs - vec2(2.0 * texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        g_out_color += textureLod(g_input_texture, norm_uvs - vec2(2.0 * texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs + vec2(2.0 * texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        g_out_color += textureLod(g_input_texture, norm_uvs + vec2(2.0 * texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs - vec2(3.0 * texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        g_out_color += textureLod(g_input_texture, norm_uvs - vec2(3.0 * texel_size.x, 0), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs + vec2(3.0 * texel_size.x, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        g_out_color += textureLod(g_input_texture, norm_uvs + vec2(3.0 * texel_size.x, 0), 0) * weight;
        normalization += weight;
    } else {
        float depth;

        depth = textureLod(g_depth_tex, norm_uvs - vec2(0, texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        g_out_color += textureLod(g_input_texture, norm_uvs - vec2(0, texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs + vec2(0, texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        g_out_color += textureLod(g_input_texture, norm_uvs + vec2(0, texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs - vec2(0, 2.0 * texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        g_out_color += textureLod(g_input_texture, norm_uvs - vec2(0, 2.0 * texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs + vec2(0, 2.0 * texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        g_out_color += textureLod(g_input_texture, norm_uvs + vec2(0, 2.0 * texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs - vec2(0, 3.0 * texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        g_out_color += textureLod(g_input_texture, norm_uvs - vec2(0, 3.0 * texel_size.y), 0) * weight;
        normalization += weight;

        depth = textureLod(g_depth_tex, norm_uvs + vec2(0, 3.0 * texel_size.y), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        g_out_color += textureLod(g_input_texture, norm_uvs + vec2(0, 3.0 * texel_size.y), 0) * weight;
        normalization += weight;
    }

    g_out_color /= normalization;
}

