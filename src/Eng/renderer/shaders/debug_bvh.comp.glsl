#version 430 core

#include "_cs_common.glsl"
#include "debug_bvh_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NODES_BUF_SLOT) uniform usamplerBuffer g_nodes_buf;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform image2D g_color_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

uint node_word(const uint node_ndx, const uint word) {
    return texelFetch(g_nodes_buf, int(node_ndx * BVH_NODE_STRIDE + word)).x;
}

vec3 node_bbox_min(const uint node_ndx) {
    return vec3(uintBitsToFloat(node_word(node_ndx, 0u)), uintBitsToFloat(node_word(node_ndx, 1u)),
                uintBitsToFloat(node_word(node_ndx, 2u)));
}

vec3 node_bbox_max(const uint node_ndx) {
    return vec3(uintBitsToFloat(node_word(node_ndx, 4u)), uintBitsToFloat(node_word(node_ndx, 5u)),
                uintBitsToFloat(node_word(node_ndx, 6u)));
}

bool node_is_leaf(const uint node_ndx) {
    return (node_word(node_ndx, BVH_NODE_LEAF_LEFT) & 1u) != 0u;
}

uint node_left_child(const uint node_ndx) {
    return node_word(node_ndx, BVH_NODE_LEAF_LEFT) >> 1u;
}

uint node_right_child(const uint node_ndx) {
    return node_word(node_ndx, BVH_NODE_AXIS_RIGHT) >> 2u;
}

void main() {
    const uvec2 px = gl_GlobalInvocationID.xy;
    if (px.x >= g_params.img_size.x || px.y >= g_params.img_size.y) {
        return;
    }

    if (g_params.root_node == BVH_NO_NODE) {
        return;
    }

    const float depth = texelFetch(g_depth_tex, ivec2(px), 0).x;
    if (depth == 0.0) {
        // Sky: cleared depth, nothing to traverse towards
        return;
    }

    const vec2 pix_uv = (vec2(px) + 0.5) / vec2(g_params.img_size);
    const vec2 clip_xy = 2.0 * pix_uv - 1.0;

    // Pixel ray as the near..far segment, same construction as rt_debug_swrt (reversed depth: 1.0 near, 0.0 far)
    const vec3 ray_origin_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, vec4(clip_xy, 1.0, 1.0));
    const vec3 ray_end_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, vec4(clip_xy, 0.0, 1.0));
    const vec3 ray_dir_ws = normalize(ray_end_ws - ray_origin_ws);

    // Surface point on the same ray, raw texel depth used as clip z directly
    const vec3 hit_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, vec4(clip_xy, depth, 1.0));
    const float ray_len = length(hit_ws - ray_origin_ws);

    const vec3 inv_d = rcp(ray_dir_ws);
    const vec3 neg_inv_d_o = -inv_d * ray_origin_ws;

    uint stack[64];
    uint stack_size = 0u;
    stack[stack_size++] = g_params.root_node;

    uint visited = 0u;
    while (stack_size != 0u) {
        const uint cur = stack[--stack_size];
        // Bounds every node_word() fetch: indices outside the uploaded snapshot can only come from
        // a tree bigger than the fixed buffer cap, or from a corrupted tree in a Release run
        if (cur >= g_params.node_count) {
            continue;
        }

        if (!bbox_test(inv_d, neg_inv_d_o, 0.0, ray_len, node_bbox_min(cur), node_bbox_max(cur))) {
            continue;
        }

        visited++;

        if (!node_is_leaf(cur)) {
            if (stack_size + 2u <= 64u) {
                stack[stack_size++] = node_left_child(cur);
                stack[stack_size++] = node_right_child(cur);
            }
        }
    }

    if (visited == 0u) {
        return;
    }

    // Visits measured against the tree's own depth: ~1.0 is what a balanced walk costs per ray,
    // above that the ray walks far more boxes than the depth implies (overlap / degenerate tree)
    const uint depth_levels = g_params.tree_depth > 0u ? g_params.tree_depth : 1u;
    const float norm = float(VISIT_DEPTH_FACTOR * depth_levels);

    // Overlay the heat map, keeping the packed alpha channel and HDR encoding
    const vec4 prev_color = imageLoad(g_color_img, ivec2(px));
    const vec3 dbg_color = heatmap(saturate(float(visited) / norm));
    imageStore(g_color_img, ivec2(px), vec4(compress_hdr(dbg_color, g_shrd_data.cam_pos_and_exp.w), prev_color.a));
}
