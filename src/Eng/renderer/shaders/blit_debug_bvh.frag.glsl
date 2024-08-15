#version 430 core

#include "_fs_common.glsl"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = 0) uniform sampler2D g_depth_tex;
layout(binding = 1) uniform samplerBuffer g_nodes_buf;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) int g_root_index;
};
#else
layout(location = 12) uniform int g_root_index;
#endif

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

void main() {
    vec2 norm_uvs = g_vtx_uvs / g_shrd_data.res_and_fres.xy;

    float depth = texelFetch(g_depth_tex, ivec2(g_vtx_uvs), 0).r;
    depth = 2.0 * depth - 1.0;

    vec4 ray_start_cs = vec4(g_vtx_uvs / g_shrd_data.res_and_fres.xy, 0.0, 1.0);
    ray_start_cs.xy = 2.0 * ray_start_cs.xy - 1.0;

    vec4 ray_end_cs = vec4(g_vtx_uvs / g_shrd_data.res_and_fres.xy, depth, 1.0);
    ray_end_cs.xy = 2.0 * ray_end_cs.xy - 1.0;

    vec4 ray_start_ws = g_shrd_data.world_from_clip * ray_start_cs;
    ray_start_ws /= ray_start_ws.w;

    vec4 ray_end_ws = g_shrd_data.world_from_clip * ray_end_cs;
    ray_end_ws /= ray_end_ws.w;

    vec3 ray_dir_ws = ray_end_ws.xyz - ray_start_ws.xyz;
    float ray_length = length(ray_dir_ws);
    ray_dir_ws /= ray_length;

    vec3 inv_dir = 1.0 / ray_dir_ws;

    int stack[32];
    int stack_size = 0;
    stack[stack_size++] = g_root_index;

    int tree_complexity = 0;

    while (stack_size != 0) {
        int cur = stack[--stack_size];

        /*
            struct bvh_node_t {
                uvec4 node_data0;   // { prim_index  (u32), prim_count  (u32), left_child  (u32), right_child (u32) }
                xvec4 node_data1;   // { bbox_min[0] (f32), bbox_min[1] (f32), bbox_min[2] (f32), parent      (u32) }
                xvec4 node_data2;   // { bbox_max[0] (f32), bbox_max[1] (f32), bbox_max[2] (f32), space_axis  (u32) }
            };
        */

        vec4 node_data1 = texelFetch(g_nodes_buf, cur * 3 + 1);
        vec4 node_data2 = texelFetch(g_nodes_buf, cur * 3 + 2);

        if (!bbox_test(ray_start_ws.xyz, inv_dir, 100.0, node_data1.xyz, node_data2.xyz)) continue;

        tree_complexity++;

        uvec4 node_data0 = floatBitsToUint(texelFetch(g_nodes_buf, cur * 3 + 0));
        if (node_data0.y == 0u) {
            stack[stack_size++] = int(node_data0.w);
            stack[stack_size++] = int(node_data0.z);
        }
    }

    g_out_color = vec4(heatmap(float(tree_complexity) / 128.0), 0.85);
}
