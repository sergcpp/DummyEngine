#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_gauss_interface.h"

layout(binding = SRC_TEX_SLOT) uniform sampler2D g_tex;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

void main() {
    g_out_color = vec4(0.0);

    if(g_params.vertical.x < 0.5) {
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(4, 0), 0) * 0.0162162162;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(3, 0), 0) * 0.0540540541;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(2, 0), 0) * 0.1216216216;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(1, 0), 0) * 0.1945945946;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs), 0) * 0.2270270270;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(1, 0), 0) * 0.1945945946;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(2, 0), 0) * 0.1216216216;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(3, 0), 0) * 0.0540540541;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(4, 0), 0) * 0.0162162162;
    } else {
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 4), 0) * 0.0162162162;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 3), 0) * 0.0540540541;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 2), 0) * 0.1216216216;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) - ivec2(0, 1), 0) * 0.1945945946;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs), 0) * 0.2270270270;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 1), 0) * 0.1945945946;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 2), 0) * 0.1216216216;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 3), 0) * 0.0540540541;
        g_out_color += texelFetch(g_tex, ivec2(g_vtx_uvs) + ivec2(0, 4), 0) * 0.0162162162;
    }
}

