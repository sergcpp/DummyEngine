#version 310 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif
        
layout(binding = 0) uniform mediump sampler2DMS s_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    vec3 col = vec3(0.0);
    for (float j = -1.5; j < 2.0; j += 1.0) {
        for (float i = -1.5; i < 2.0; i += 1.0) {
            col += texelFetch(s_texture, ivec2(aVertexUVs_ + vec2(i, j)), 0).xyz;
        }
    }
    outColor = vec4((1.0/16.0) * col, 1.0);
}
