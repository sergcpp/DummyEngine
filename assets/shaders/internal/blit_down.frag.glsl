#version 310 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_down_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = SRC_TEX_SLOT) uniform sampler2D s_texture;

LAYOUT(location = 0) in vec2 aVertexUVs_;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 norm_uvs = aVertexUVs_;
    vec2 px_offset = 1.0 / g_params.resolution.xy;

    vec2 sample_positions[4];
    sample_positions[0] = norm_uvs - px_offset;
    sample_positions[1] = norm_uvs + vec2(px_offset.x, -px_offset.y);
    sample_positions[2] = norm_uvs + px_offset;
    sample_positions[3] = norm_uvs + vec2(-px_offset.x, px_offset.y);

    vec3 color = vec3(0.0);
    color += textureLod(s_texture, sample_positions[0], 0.0).rgb;
    color += textureLod(s_texture, sample_positions[1], 0.0).rgb;
    color += textureLod(s_texture, sample_positions[2], 0.0).rgb;
    color += textureLod(s_texture, sample_positions[3], 0.0).rgb;
    color /= 4.0;

    outColor = vec4(color, 1.0);
}
