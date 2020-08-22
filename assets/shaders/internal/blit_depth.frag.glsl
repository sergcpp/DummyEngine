#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;
layout(location = 1) uniform float near;
layout(location = 2) uniform float far;
layout(location = 3) uniform vec3 color;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    float depth = texelFetch(s_texture, ivec2(aVertexUVs_), 0).r;
    if (near > 0.0001) {
        // cam is not orthographic
        depth = (near * far) / (depth * (near - far) + far);
        depth /= far;
    }
    outColor = vec4(vec3(depth) * color, 1.0);
}
