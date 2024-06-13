#version 430 core

#include "_fs_common.glsl"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) vec4 g_dof_equation;
};
#else
layout(location = 1) uniform vec4 g_dof_equation;
#endif

layout(binding = BIND_BASE0_TEX) uniform sampler2D g_depth;

#if defined(VULKAN)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out float outCoc;

void main() {
    vec4 depth = textureGather(g_depth, g_vtx_uvs, 0);
    vec4 coc = clamp(g_dof_equation.x * depth + g_dof_equation.y, 0.0, 1.0);

    float max_coc = max(max(coc.x, coc.y), max(coc.z, coc.w));
    outCoc = max_coc;
}
