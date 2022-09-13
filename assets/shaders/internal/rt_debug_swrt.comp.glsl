#version 310 es

#include "_swrt_common.glsl"

#include "rt_debug_interface.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(std430, binding = NODES_BUF_SLOT) readonly buffer Nodes {
    bvh_node_t g_nodes[];
};

layout(std430, binding = VTX_BUF1_SLOT) readonly buffer VtxData0 {
    vec4 g_vtx_data0[];
};

layout(std430, binding = VTX_BUF2_SLOT) readonly buffer VtxData1 {
    uvec4 g_vtx_data1[];
};

layout(std430, binding = NDX_BUF_SLOT) readonly buffer VtxNdxData {
    uint g_vtx_indices[];
};

layout(std430, binding = PRIM_NDX_BUF_SLOT) readonly buffer PrimNdxData {
    uint g_prim_indices[];
};

layout(binding = OUT_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_image;

shared uint g_stack[LOCAL_GROUP_SIZE_X * LOCAL_GROUP_SIZE_Y][MAX_STACK_SIZE];

const int FirstVertex = 2097216;
//const int IndicesOffset = 384;

void IntersectTris_ClosestHit(vec3 ro, vec3 rd, int tri_start, int tri_end, int obj_index,
                              inout hit_data_t out_inter) {
    for (int i = tri_start; i < tri_end; ++i) {
        uint j = g_prim_indices[i];

        float t, u, v;
        if (IntersectTri(ro, rd, g_vtx_data0[FirstVertex + g_vtx_indices[3 * j + 0]].xyz,
                                 g_vtx_data0[FirstVertex + g_vtx_indices[3 * j + 1]].xyz,
                                 g_vtx_data0[FirstVertex + g_vtx_indices[3 * j + 2]].xyz, t, u, v) && t > 0.0 && t < out_inter.t) {
            out_inter.mask = -1;
            out_inter.prim_index = int(j);
            out_inter.obj_index = obj_index;
            out_inter.t = t;
            out_inter.u = u;
            out_inter.v = v;
        }
    }
}

void Traverse_MicroTree_WithStack(vec3 ro, vec3 rd, vec3 inv_d, int obj_index, uint node_index,
                                  uint stack_size, inout hit_data_t inter) {
    vec3 neg_inv_do = -inv_d * ro;

    uint initial_stack_size = stack_size;
    g_stack[gl_LocalInvocationIndex][stack_size++] = node_index;

    while (stack_size != initial_stack_size) {
        uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];

        bvh_node_t n = g_nodes[cur];

        if (!_bbox_test_fma(inv_d, neg_inv_do, inter.t, n.bbox_min.xyz, n.bbox_max.xyz)) {
            continue;
        }

        if ((floatBitsToUint(n.bbox_min.w) & LEAF_NODE_BIT) == 0) {
            g_stack[gl_LocalInvocationIndex][stack_size++] = far_child(rd, n);
            g_stack[gl_LocalInvocationIndex][stack_size++] = near_child(rd, n);
        } else {
            int tri_start = int(floatBitsToUint(n.bbox_min.w) & PRIM_INDEX_BITS);
            int tri_end = tri_start + floatBitsToInt(n.bbox_max.w);

            IntersectTris_ClosestHit(ro, rd, tri_start, tri_end, obj_index, inter);
        }
    }
}

/*void Traverse_MacroTree_WithStack(vec3 orig_ro, vec3 orig_rd, vec3 orig_inv_rd, uint node_index,
                                  inout hit_data_t inter) {
    vec3 orig_neg_inv_do = -orig_inv_rd * orig_ro;

    uint stack_size = 0;
    g_stack[gl_LocalInvocationIndex][stack_size++] = node_index;

    while (stack_size != 0) {
        uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];

        bvh_node_t n = g_nodes[cur];

        if (!_bbox_test_fma(orig_inv_rd, orig_neg_inv_do, inter.t, n.bbox_min.xyz, n.bbox_max.xyz)) {
            continue;
        }

        if ((floatBitsToUint(n.bbox_min.w) & LEAF_NODE_BIT) == 0) {
            g_stack[gl_LocalInvocationIndex][stack_size++] = far_child(orig_rd, n);
            g_stack[gl_LocalInvocationIndex][stack_size++] = near_child(orig_rd, n);
        } else {
            uint prim_index = (floatBitsToUint(n.bbox_min.w) & PRIM_INDEX_BITS);
            uint prim_count = floatBitsToUint(n.bbox_max.w);
            for (uint i = prim_index; i < prim_index + prim_count; ++i) {
                mesh_instance_t mi = g_mesh_instances[g_mi_indices[i]];
                mesh_t m = g_meshes[floatBitsToUint(mi.bbox_max.w)];
                transform_t tr = g_transforms[floatBitsToUint(mi.bbox_min.w)];

                if (!_bbox_test_fma(orig_inv_rd, orig_neg_inv_do, inter.t, mi.bbox_min.xyz, mi.bbox_max.xyz)) {
                    continue;
                }

                vec3 ro = (tr.inv_xform * vec4(orig_ro, 1.0)).xyz;
                vec3 rd = (tr.inv_xform * vec4(orig_rd, 0.0)).xyz;
                vec3 inv_d = safe_invert(rd);

                Traverse_MicroTree_WithStack(ro, rd, inv_d, int(g_mi_indices[i]), m.node_index,
                                             stack_size, inter);
            }
        }
    }
}*/

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

int hash(const int x) {
    uint ret = uint(x);
    ret = ((ret >> 16) ^ ret) * 0x45d9f3b;
    ret = ((ret >> 16) ^ ret) * 0x45d9f3b;
    ret = (ret >> 16) ^ ret;
    return int(ret);
}

float construct_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    const float  f = uintBitsToFloat(m);   // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const vec2 px_center = vec2(gl_GlobalInvocationID.xy) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(g_params.img_size);
    vec2 d = in_uv * 2.0 - 1.0;
    d.y = -d.y;

    vec4 origin = g_shrd_data.inv_view_matrix * vec4(0, 0, 0, 1);
    origin /= origin.w;
    vec4 target = g_shrd_data.inv_proj_matrix * vec4(d.xy, 1, 1);
    target /= target.w;
    vec4 direction = g_shrd_data.inv_view_matrix * vec4(normalize(target.xyz), 0);

    vec3 inv_d = safe_invert(direction.xyz);

    hit_data_t inter;
    inter.mask = 0;
    inter.obj_index = inter.prim_index = 0;
    inter.t = MAX_DIST;
    inter.u = inter.v = 0.0;

    Traverse_MicroTree_WithStack(origin.xyz, direction.xyz, inv_d, 0, 0, 0, inter);

    vec3 col = vec3(0.0, 0.5, 0.5);
    if (inter.mask != 0) {
        col.r = construct_float(hash(inter.prim_index));
        col.g = construct_float(hash(hash(inter.prim_index)));
        col.b = construct_float(hash(hash(hash(inter.prim_index))));
    }
    imageStore(g_out_image, ivec2(gl_GlobalInvocationID.xy), vec4(col, 1.0));
}