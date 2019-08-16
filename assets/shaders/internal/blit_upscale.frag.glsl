#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "blit_upscale_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D depth_texture;
layout(binding = DEPTH_LOW_TEX_SLOT) uniform mediump sampler2D depth_low_texture;
layout(binding = INPUT_TEX_SLOT) uniform lowp sampler2D input_texture;

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

LAYOUT(location = 0) in highp vec2 aVertexUVs_;
layout(location = 0) out vec4 outColor;

void main() {
	vec2 norm_uvs = aVertexUVs_;
	vec2 texel_size_low = vec2(0.5) / params.resolution.zw;

    float d0 = LinearizeDepth(textureLod(depth_texture, norm_uvs, 0.0).r, params.clip_info);
 
    float d1 = abs(d0 - textureLod(depth_low_texture, norm_uvs, 0.0).r);
    float d2 = abs(d0 - textureLod(depth_low_texture, norm_uvs + vec2(0, texel_size_low.y), 0.0).r);
    float d3 = abs(d0 - textureLod(depth_low_texture, norm_uvs + vec2(texel_size_low.x, 0), 0.0).r);
    float d4 = abs(d0 - textureLod(depth_low_texture, norm_uvs + texel_size_low, 0.0).r);
 
    float dmin = min(min(d1, d2), min(d3, d4));
 
    if (dmin < 0.05) {
        outColor = textureLod(input_texture, norm_uvs * params.resolution.xy / params.resolution.zw, 0.0);
    } else {
        if (dmin == d1) {
            outColor = textureLod(input_texture, norm_uvs, 0.0);
        }else if (dmin == d2) {
            outColor = textureLod(input_texture, norm_uvs + vec2(0, texel_size_low.y), 0.0);
        } else if (dmin == d3) {
            outColor = textureLod(input_texture, norm_uvs + vec2(texel_size_low.x, 0), 0.0);
        } else if (dmin == d4) {
            outColor = textureLod(input_texture, norm_uvs + texel_size_low, 0.0);
        }
    }
}
