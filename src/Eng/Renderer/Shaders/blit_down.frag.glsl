R"(#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

)"
#include "_fs_common.glsl"
R"(

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

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
)"