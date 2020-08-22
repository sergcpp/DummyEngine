#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    vec2 norm_uvs = aVertexUVs_;
    vec2 px_offset = 1.0 / shrd_data.uResAndFRes.xy;

    vec2 sample_positions[4];
    sample_positions[0] = norm_uvs - px_offset;
    sample_positions[1] = norm_uvs + vec2(px_offset.x, -px_offset.y);
    sample_positions[2] = norm_uvs + px_offset;
    sample_positions[3] = norm_uvs + vec2(-px_offset.x, px_offset.y);

    mediump vec3 color = vec3(0.0);
    color += textureLod(s_texture, sample_positions[0], 0.0).rgb;
    color += textureLod(s_texture, sample_positions[1], 0.0).rgb;
    color += textureLod(s_texture, sample_positions[2], 0.0).rgb;
    color += textureLod(s_texture, sample_positions[3], 0.0).rgb;
    color /= 4.0;

    outColor = vec4(color, 1.0);
}
