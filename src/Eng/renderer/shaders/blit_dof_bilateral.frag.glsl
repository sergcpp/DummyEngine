#version 430 core

layout(binding = 0) uniform sampler2D g_depth_tex;
layout(binding = 1) uniform sampler2D g_source_tex;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) float vertical;
                        float ref_depth;
};
#else
layout(location = 1) uniform float vertical;
layout(location = 2) uniform float ref_depth;
#endif

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

void main() {
    ivec2 icoord = ivec2(g_vtx_uvs);

    //float center_depth = texelFetch(g_depth_tex, icoord, 0).r;
    float cmp_depth = ref_depth;
    float closeness = 1.0;
    float weight = closeness * 0.214607;
    g_out_color = vec4(0.0);
    g_out_color += texelFetch(g_source_tex, icoord, 0) * weight;

    float normalization = weight;

    // 0.071303 0.131514 0.189879 0.214607

    if(vertical < 0.5) {
        float depth, diff;

        depth = texelFetch(g_depth_tex, icoord - ivec2(1, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.189879;
        g_out_color += texelFetch(g_source_tex, icoord - ivec2(1, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord + ivec2(1, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.189879;
        g_out_color += texelFetch(g_source_tex, icoord + ivec2(1, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord - ivec2(2, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.131514;
        g_out_color += texelFetch(g_source_tex, icoord - ivec2(2, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord + ivec2(2, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.131514;
        g_out_color += texelFetch(g_source_tex, icoord + ivec2(2, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord - ivec2(3, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.071303;
        g_out_color += texelFetch(g_source_tex, icoord - ivec2(3, 0), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord + ivec2(3, 0), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.071303;
        g_out_color += texelFetch(g_source_tex, icoord + ivec2(3, 0), 0) * weight;
        normalization += weight;
    } else {
        float depth, diff;

        depth = texelFetch(g_depth_tex, icoord - ivec2(0, 1), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.189879;
        g_out_color += texelFetch(g_source_tex, icoord - ivec2(0, 1), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord + ivec2(0, 1), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.189879;
        g_out_color += texelFetch(g_source_tex, icoord + ivec2(0, 1), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord - ivec2(0, 2), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.131514;
        g_out_color += texelFetch(g_source_tex, icoord - ivec2(0, 2), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord + ivec2(0, 2), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.131514;
        g_out_color += texelFetch(g_source_tex, icoord + ivec2(0, 2), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord - ivec2(0, 3), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.071303;
        g_out_color += texelFetch(g_source_tex, icoord - ivec2(0, 3), 0) * weight;
        normalization += weight;

        depth = texelFetch(g_depth_tex, icoord + ivec2(0, 3), 0).r;
        diff = abs(depth - cmp_depth);
        closeness = clamp(diff * diff, 0.0, 1.0);
        weight = closeness * 0.071303;
        g_out_color += texelFetch(g_source_tex, icoord + ivec2(0, 3), 0) * weight;
        normalization += weight;
    }

    g_out_color /= normalization;
}

