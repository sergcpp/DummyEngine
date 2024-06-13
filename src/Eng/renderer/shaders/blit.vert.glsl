#version 320 es

#include "_vs_common.glsl"

layout(location = VTX_POS_LOC) in vec2 g_in_vtx_pos;
layout(location = VTX_UV1_LOC) in vec2 g_in_vtx_uvs;

/*#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    vec4 uTransform;
};
#else
layout(location = 0) uniform vec4 uTransform;
#endif*/

#if defined(VULKAN)
layout(location = 0)
#endif
out vec2 g_vtx_uvs;


void main() {
    g_vtx_uvs = g_in_vtx_uvs;//uTransform.xy + g_in_vtx_uvs * uTransform.zw;
    gl_Position = vec4(g_in_vtx_pos, 0.5, 1.0);
}
