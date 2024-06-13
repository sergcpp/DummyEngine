#version 430 core

#include "_fs_common.glsl"

layout(binding = BIND_BASE0_TEX) uniform sampler2D g_tex;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) vec3 color;
                        float near;
                        float far;
};
#else
layout(location = 3) uniform vec3 color;
layout(location = 1) uniform float near;
layout(location = 2) uniform float far;
#endif

#if defined(VULKAN)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    float depth = texelFetch(g_tex, ivec2(g_vtx_uvs), 0).r;
    if (near > 0.0001) {
        // cam is not orthographic
        depth = (near * far) / (depth * (near - far) + far);
        depth /= far;
    }
    g_out_color = vec4(vec3(depth) * color, 1.0);
}
