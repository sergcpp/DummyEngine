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

layout(location = 1) uniform vec4 uDofEquation;

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_depth;

in vec2 aVertexUVs_;

layout(location = 0) out float outCoc;

void main() {
    vec4 depth = textureGather(s_depth, aVertexUVs_, 0);
    vec4 coc = clamp(uDofEquation.x * depth + uDofEquation.y, 0.0, 1.0);

    float max_coc = max(max(coc.x, coc.y), max(coc.z, coc.w));
    outCoc = max_coc;
}
)"