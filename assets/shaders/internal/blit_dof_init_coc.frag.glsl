#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
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

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) vec4 uDofEquation;
};
#else
layout(location = 1) uniform vec4 uDofEquation;
#endif

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_depth;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out float outCoc;

void main() {
    vec4 depth = textureGather(s_depth, aVertexUVs_, 0);
    vec4 coc = clamp(uDofEquation.x * depth + uDofEquation.y, 0.0, 1.0);

    float max_coc = max(max(coc.x, coc.y), max(coc.z, coc.w));
    outCoc = max_coc;
}
