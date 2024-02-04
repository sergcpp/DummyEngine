#version 310 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

layout(binding = 0) uniform mediump sampler2DMS g_tex;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    vec3 col = vec3(0.0);
    for (float j = -1.5; j < 2.0; j += 1.0) {
        for (float i = -1.5; i < 2.0; i += 1.0) {
            col += texelFetch(g_tex, ivec2(g_vtx_uvs + vec2(i, j)), 0).xyz;
        }
    }
    g_out_color = vec4((1.0/16.0) * col, 1.0);
}
