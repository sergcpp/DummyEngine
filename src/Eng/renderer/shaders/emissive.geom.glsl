#version 430 core
#if !defined(VULKAN) && !defined(NO_BINDLESS)
#extension GL_ARB_bindless_texture : enable
#endif

#include "_common.glsl"

#define NO_BINDING_DECLARATION
#include "texturing_common.glsl"

#pragma multi_compile _ NO_BINDLESS

layout(triangles) in;
in gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 g_vtx_pos[];
layout(location = 1) in vec2 g_vtx_uvs[];
#if !defined(NO_BINDLESS)
    layout(location = 4) in flat TEX_HANDLE g_emission_tex[];
#endif // !NO_BINDLESS
layout(location = 5) in flat vec4 g_mat_params2[];

layout(location = 0) out flat float g_geo_area;
layout(location = 1) out vec2 g_geo_uvs;
#if !defined(NO_BINDLESS)
    layout(location = 4) out flat TEX_HANDLE g_geo_emission_tex;
#endif // !NO_BINDLESS
layout(location = 5) out flat vec4 g_geo_mat_params2;

invariant gl_Position;

void main() {
    const vec3 e1 = g_vtx_pos[1] - g_vtx_pos[0], e2 = g_vtx_pos[2] - g_vtx_pos[0];
    const float light_fwd_len = length(cross(e1, e2));

    for (int i = 0; i < 3; ++i) {
        gl_Position = gl_in[i].gl_Position;
        g_geo_area = 0.5 * light_fwd_len;
        g_geo_uvs = g_vtx_uvs[i];
    #if !defined(NO_BINDLESS)
        g_geo_emission_tex = g_emission_tex[i];
    #endif
        g_geo_mat_params2 = g_mat_params2[i];
        EmitVertex();
    }
    EndPrimitive();
}
