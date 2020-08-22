#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_texture;

layout(location = 0) uniform highp vec4 uTransform;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    vec2 norm_uvs = aVertexUVs_ / uTransform.zw;
    vec2 px_offset = 0.5 / uTransform.zw;

    vec4 color = vec4(0.0);
    color += textureLod(s_texture, norm_uvs - px_offset, 0.0);
    color += textureLod(s_texture, norm_uvs + vec2(px_offset.x, -px_offset.y), 0.0);
    color += textureLod(s_texture, norm_uvs + px_offset, 0.0);
    color += textureLod(s_texture, norm_uvs + vec2(-px_offset.x, px_offset.y), 0.0);
    color /= 4.0;

    outColor = color;
}
