#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = 0) uniform mediump sampler2D depth_texture;
layout(binding = 1) uniform lowp sampler2D source_texture;
layout(location = 1) uniform float vertical;
layout(location = 2) uniform float ref_depth;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    ivec2 icoord = ivec2(aVertexUVs_);

    //float center_depth = texelFetch(depth_texture, icoord, 0).r;
    float cmp_depth = ref_depth;
    float closeness = 1.0;
    float weight = closeness * 0.214607;
    outColor = vec4(0.0);
    outColor += texelFetch(source_texture, icoord, 0) * weight;

    float normalization = weight;

    // 0.071303 0.131514 0.189879 0.214607

    if(vertical < 0.5) {
        float depth, diff;

        depth = texelFetch(depth_texture, icoord - ivec2(1, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.189879;
        outColor += texelFetch(source_texture, icoord - ivec2(1, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(1, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.189879;
        outColor += texelFetch(source_texture, icoord + ivec2(1, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord - ivec2(2, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.131514;
        outColor += texelFetch(source_texture, icoord - ivec2(2, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(2, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.131514;
        outColor += texelFetch(source_texture, icoord + ivec2(2, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord - ivec2(3, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.071303;
        outColor += texelFetch(source_texture, icoord - ivec2(3, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(3, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.071303;
        outColor += texelFetch(source_texture, icoord + ivec2(3, 0), 0) * weight;
        normalization += weight;
    } else {
        float depth, diff;

        depth = texelFetch(depth_texture, icoord - ivec2(0, 1), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.189879;
        outColor += texelFetch(source_texture, icoord - ivec2(0, 1), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(0, 1), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.189879;
        outColor += texelFetch(source_texture, icoord + ivec2(0, 1), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord - ivec2(0, 2), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.131514;
        outColor += texelFetch(source_texture, icoord - ivec2(0, 2), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(0, 2), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.131514;
        outColor += texelFetch(source_texture, icoord + ivec2(0, 2), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord - ivec2(0, 3), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.071303;
        outColor += texelFetch(source_texture, icoord - ivec2(0, 3), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(0, 3), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.071303;
        outColor += texelFetch(source_texture, icoord + ivec2(0, 3), 0) * weight;
        normalization += weight;
    }

    outColor /= normalization;
}

