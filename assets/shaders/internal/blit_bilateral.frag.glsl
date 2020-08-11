#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = 0) uniform mediump sampler2D depth_texture;
layout(binding = 1) uniform lowp sampler2D source_texture;
layout(location = 3) uniform float vertical;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    ivec2 icoord = ivec2(aVertexUVs_);

    float center_depth = texelFetch(depth_texture, icoord, 0).r;
    float closeness = 1.0 / (0.075 + 0.0);
    float weight = closeness * 0.214607;
    outColor = vec4(0.0);
    outColor += texelFetch(source_texture, icoord, 0) * weight;

    float normalization = weight;

    if(vertical < 0.5) {
        float depth;

        // 0.071303 0.131514 0.189879 0.214607

        depth = texelFetch(depth_texture, icoord - ivec2(1, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        outColor += texelFetch(source_texture, icoord - ivec2(1, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(1, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        outColor += texelFetch(source_texture, icoord + ivec2(1, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord - ivec2(2, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        outColor += texelFetch(source_texture, icoord - ivec2(2, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(2, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        outColor += texelFetch(source_texture, icoord + ivec2(2, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord - ivec2(3, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        outColor += texelFetch(source_texture, icoord - ivec2(3, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(3, 0), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        outColor += texelFetch(source_texture, icoord + ivec2(3, 0), 0) * weight;
        normalization += weight;
    } else {
        float depth;

        depth = texelFetch(depth_texture, icoord - ivec2(0, 1), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        outColor += texelFetch(source_texture, icoord - ivec2(0, 1), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(0, 1), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.189879;
        outColor += texelFetch(source_texture, icoord + ivec2(0, 1), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord - ivec2(0, 2), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        outColor += texelFetch(source_texture, icoord - ivec2(0, 2), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(0, 2), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.131514;
        outColor += texelFetch(source_texture, icoord + ivec2(0, 2), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord - ivec2(0, 3), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        outColor += texelFetch(source_texture, icoord - ivec2(0, 3), 0) * weight;
        normalization += weight;

        depth = texelFetch(depth_texture, icoord + ivec2(0, 3), 0).r;
        closeness = 1.0 / (0.075 + abs(center_depth - depth));
        weight = closeness * 0.071303;
        outColor += texelFetch(source_texture, icoord + ivec2(0, 3), 0) * weight;
        normalization += weight;
    }

    outColor /= normalization;
}

