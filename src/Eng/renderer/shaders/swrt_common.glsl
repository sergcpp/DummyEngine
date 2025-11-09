#ifndef SWRT_COMMON_GLSL
#define SWRT_COMMON_GLSL

#include "_cs_common.glsl"

#define FLT_MAX 3.402823466e+38

#define IGNORE_BACKFACING 0

#ifndef SWRT_SUBGROUP
    #define SWRT_SUBGROUP 0
#endif

int IntersectTri(vec3 o, vec3 d, vec3 v0, vec3 v1, vec3 v2, out float t, out float u, out float v) {
    vec3 e10 = v1 - v0;
    vec3 e20 = v2 - v0;

    vec3 pvec = cross(d, e20);
    float det = dot(e10, pvec);
#if IGNORE_BACKFACING
    if (det < 0.000000005) {
        return 0;
    }

    vec3 tvec = o - v0;
    u = dot(tvec, pvec);
    if (u < 0.0 || u > det) {
        return 0;
    }

    vec3 qvec = cross(tvec, e10);
    v = dot(d, qvec);
    if (v < 0.0 || u + v > det) {
        return 0;
    }

    t = dot(e20, qvec);

    float inv_det = 1.0 / det;

    t *= inv_det;
    u *= inv_det;
    v *= inv_det;

    return 1;
#else
    if (det > -0.000000005 && det < 0.000000005) {
        return 0;
    }

    float inv_det = 1.0 / det;

    vec3 tvec = o - v0;
    u = dot(tvec, pvec) * inv_det;
    if (u < 0.0 || u > 1.0) {
        return 0;
    }

    vec3 qvec = cross(tvec, e10);
    v = dot(d, qvec) * inv_det;
    if (v < 0.0 || u + v > 1.0) {
        return 0;
    }

    t = dot(e20, qvec) * inv_det;

    return det > 0.0 ? 1 : -1;
#endif
}

#undef IGNORE_BACKFACING

vec3 safe_invert(vec3 v) {
    vec3 ret;
    for (int i = 0; i < 3; ++i) {
        //ret[i] = (abs(v[i]) > FLT_EPS) ? (1.0 / v[i]) : copysign(FLT_MAX, v[i]);
        ret[i] = 1.0 / ((abs(v[i]) > FLT_EPS) ? v[i] : copysign(FLT_EPS, v[i]));
    }
    return ret;
}

const uint LEAF_NODE_BIT = (1u << 31);
const uint PRIM_INDEX_BITS = ~LEAF_NODE_BIT;
const uint LEFT_CHILD_BITS = ~LEAF_NODE_BIT;

const uint SEP_AXIS_BITS = (3u << 30); // 0b11u
const uint PRIM_COUNT_BITS = ~SEP_AXIS_BITS;
const uint RIGHT_CHILD_BITS = ~SEP_AXIS_BITS;

const uint BVH2_PRIM_COUNT_BITS = (7u << 29); // 0b111u
const uint BVH2_PRIM_INDEX_BITS = ~BVH2_PRIM_COUNT_BITS;

struct bvh_node_t {
    vec4 bbox_min; // w is prim_index/left_child
    vec4 bbox_max; // w is prim_count/right_child
};

struct bvh2_node_t {
    vec4 ch_data0;  // [ ch0.min.x, ch0.max.x, ch0.min.y, ch0.max.y ]
    vec4 ch_data1;  // [ ch1.min.x, ch1.max.x, ch1.min.y, ch1.max.y ]
    vec4 ch_data2;  // [ ch0.min.z, ch0.max.z, ch1.min.z, ch1.max.z ]
    uvec4 ch_data3; // x - left child, y - right child
};

struct mesh_instance_t {
    vec4 bbox_min; // w is geo_index
    vec4 bbox_max; // w is mesh_index
    uvec4 flags;   // x is ray visibility
    mat3x4 inv_transform;
    mat3x4 transform;
};

struct hit_data_t {
    int mask;
    int obj_index;
    uint geo_index_count;
    int prim_index;
    float tmin, tmax, u, v;
};

#define near_child(rd, n)   \
    (rd)[floatBitsToUint(n.bbox_max.w) >> 30] < 0 ? (floatBitsToUint(n.bbox_max.w) & RIGHT_CHILD_BITS) : floatBitsToUint(n.bbox_min.w)

#define far_child(rd, n)    \
    (rd)[floatBitsToUint(n.bbox_max.w) >> 30] < 0 ? floatBitsToUint(n.bbox_min.w) : (floatBitsToUint(n.bbox_max.w) & RIGHT_CHILD_BITS)

void IntersectTris_ClosestHit(samplerBuffer vtx_positions, usamplerBuffer vtx_indices, usamplerBuffer prim_indices,
                              vec3 ro, vec3 rd, int tri_start, int tri_end, uint first_vertex,
                              int obj_index, inout hit_data_t out_inter, uint geo_index_count) {
    for (int i = tri_start; i < tri_end; ++i) {
        const int j = int(texelFetch(prim_indices, i).x);

        const uint i0 = texelFetch(vtx_indices, 3 * j + 0).x;
        const uint i1 = texelFetch(vtx_indices, 3 * j + 1).x;
        const uint i2 = texelFetch(vtx_indices, 3 * j + 2).x;

        float t, u, v;
        const int inter_result = IntersectTri(ro, rd, texelFetch(vtx_positions, int(first_vertex + i0)).xyz,
                                                      texelFetch(vtx_positions, int(first_vertex + i1)).xyz,
                                                      texelFetch(vtx_positions, int(first_vertex + i2)).xyz, t, u, v);
        if (inter_result != 0 && t > out_inter.tmin && t < out_inter.tmax) {
            out_inter.mask = -1;
            out_inter.prim_index = inter_result > 0 ? int(j) : -int(j) - 1;
            out_inter.obj_index = obj_index;
            out_inter.geo_index_count = geo_index_count;
            out_inter.tmax = t;
            out_inter.u = u;
            out_inter.v = v;
        }
    }
}

#ifndef MAX_STACK_SIZE
    #define MAX_STACK_SIZE 32
#endif

shared uint g_stack[64][MAX_STACK_SIZE];

void Traverse_BLAS_WithStack(samplerBuffer blas_nodes, samplerBuffer vtx_positions, usamplerBuffer vtx_indices, usamplerBuffer prim_indices,
                             vec3 ro, vec3 rd, vec3 inv_d, int obj_index, uint node_index,
                             uint first_vertex, uint stack_size, inout hit_data_t inter, uint geo_index_count) {
    const vec3 neg_inv_do = -inv_d * ro;

    const uint initial_stack_size = stack_size;
    g_stack[gl_LocalInvocationIndex][stack_size++] = 0x1fffffffu;

    uint cur = node_index;
    while (stack_size != initial_stack_size) {
        uint leaf_node = 0;
        while (stack_size != initial_stack_size && (cur & BVH2_PRIM_COUNT_BITS) == 0) {
            const vec4 ch_data0 = texelFetch(blas_nodes, int(BVH2_NODE_BUF_STRIDE * cur + 0));
            const vec4 ch_data1 = texelFetch(blas_nodes, int(BVH2_NODE_BUF_STRIDE * cur + 1));
            const vec4 ch_data2 = texelFetch(blas_nodes, int(BVH2_NODE_BUF_STRIDE * cur + 2));
            const uvec4 ch_data3 = floatBitsToUint(texelFetch(blas_nodes, int(BVH2_NODE_BUF_STRIDE * cur + 3)));

            uvec2 children = ch_data3.xy;

            const vec3 ch0_min = vec3(ch_data0[0], ch_data0[2], ch_data2[0]);
            const vec3 ch0_max = vec3(ch_data0[1], ch_data0[3], ch_data2[1]);

            const vec3 ch1_min = vec3(ch_data1[0], ch_data1[2], ch_data2[2]);
            const vec3 ch1_max = vec3(ch_data1[1], ch_data1[3], ch_data2[3]);

            float ch0_dist, ch1_dist;
            const bool ch0_res = bbox_test(inv_d, neg_inv_do, inter.tmin, inter.tmax, ch0_min, ch0_max, ch0_dist);
            const bool ch1_res = bbox_test(inv_d, neg_inv_do, inter.tmin, inter.tmax, ch1_min, ch1_max, ch1_dist);

            if (!ch0_res && !ch1_res) {
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            } else {
                cur = ch0_res ? children[0] : children[1];
                if (ch0_res && ch1_res) {
                    if (ch1_dist < ch0_dist) {
                        const uint temp = cur;
                        cur = children[1];
                        children[1] = temp;
                    }
                    g_stack[gl_LocalInvocationIndex][stack_size++] = children[1];
                }
            }
            if ((cur & BVH2_PRIM_COUNT_BITS) != 0 && (leaf_node & BVH2_PRIM_COUNT_BITS) == 0) {
                leaf_node = cur;
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            }
#if SWRT_SUBGROUP
            if (subgroupAll((leaf_node & BVH2_PRIM_COUNT_BITS) != 0)) {
                break;
            }
#else
            if ((leaf_node & BVH2_PRIM_COUNT_BITS) != 0) {
                break;
            }
#endif
        }
        while ((leaf_node & BVH2_PRIM_COUNT_BITS) != 0) {
            const int tri_beg = int(leaf_node & BVH2_PRIM_INDEX_BITS),
                      tri_end = int(tri_beg + ((leaf_node & BVH2_PRIM_COUNT_BITS) >> 29) + 1);

            IntersectTris_ClosestHit(vtx_positions, vtx_indices, prim_indices, ro, rd,
                                     tri_beg, tri_end, first_vertex, obj_index, inter, geo_index_count);

            leaf_node = cur;
            if ((cur & BVH2_PRIM_COUNT_BITS) != 0) {
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            }
        }
    }
}


void Traverse_TLAS_WithStack(samplerBuffer tlas_nodes, samplerBuffer blas_nodes, samplerBuffer mesh_instances, samplerBuffer vtx_positions,
                             usamplerBuffer vtx_indices, usamplerBuffer prim_indices, vec3 orig_ro, vec3 orig_rd, vec3 orig_inv_rd, uint ray_flags, uint node_index,
                             inout hit_data_t inter) {
    const vec3 orig_neg_inv_do = -orig_inv_rd * orig_ro;

    uint stack_size = 0;
    g_stack[gl_LocalInvocationIndex][stack_size++] = 0x1fffffffu;

    uint cur = node_index;
    while (stack_size != 0) {
        uint leaf_node = 0;
        while (stack_size != 0 && (cur & BVH2_PRIM_COUNT_BITS) == 0) {
            const vec4 ch_data0 = texelFetch(tlas_nodes, int(BVH2_NODE_BUF_STRIDE * cur + 0));
            const vec4 ch_data1 = texelFetch(tlas_nodes, int(BVH2_NODE_BUF_STRIDE * cur + 1));
            const vec4 ch_data2 = texelFetch(tlas_nodes, int(BVH2_NODE_BUF_STRIDE * cur + 2));
            const uvec4 ch_data3 = floatBitsToUint(texelFetch(tlas_nodes, int(BVH2_NODE_BUF_STRIDE * cur + 3)));

            uvec2 children = ch_data3.xy;

            const vec3 ch0_min = vec3(ch_data0[0], ch_data0[2], ch_data2[0]);
            const vec3 ch0_max = vec3(ch_data0[1], ch_data0[3], ch_data2[1]);

            const vec3 ch1_min = vec3(ch_data1[0], ch_data1[2], ch_data2[2]);
            const vec3 ch1_max = vec3(ch_data1[1], ch_data1[3], ch_data2[3]);

            float ch0_dist, ch1_dist;
            const bool ch0_res = bbox_test(orig_inv_rd, orig_neg_inv_do, inter.tmin, inter.tmax, ch0_min, ch0_max, ch0_dist);
            const bool ch1_res = bbox_test(orig_inv_rd, orig_neg_inv_do, inter.tmin, inter.tmax, ch1_min, ch1_max, ch1_dist);

            if (!ch0_res && !ch1_res) {
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            } else {
                cur = ch0_res ? children[0] : children[1];
                if (ch0_res && ch1_res) {
                    if (ch1_dist < ch0_dist) {
                        const uint temp = cur;
                        cur = children[1];
                        children[1] = temp;
                    }
                    g_stack[gl_LocalInvocationIndex][stack_size++] = children[1];
                }
            }
            if ((cur & BVH2_PRIM_COUNT_BITS) != 0 && (leaf_node & BVH2_PRIM_COUNT_BITS) == 0) {
                leaf_node = cur;
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            }
#if SWRT_SUBGROUP
            if (subgroupAll((leaf_node & BVH2_PRIM_COUNT_BITS) != 0)) {
                break;
            }
#else
            if ((leaf_node & BVH2_PRIM_COUNT_BITS) != 0) {
                break;
            }
#endif
        }
        while ((leaf_node & BVH2_PRIM_COUNT_BITS) != 0) {
            const uint mi_index = (leaf_node & BVH2_PRIM_INDEX_BITS);

            const uvec4 mi_data = floatBitsToUint(texelFetch(mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * mi_index + 0)));
            const uint mi_visibility = mi_data.x, mi_node_index = mi_data.y, mi_vindex = mi_data.z, mi_geo_index_count = mi_data.w;
            if ((mi_visibility & ray_flags) != 0) {
                const mat4x3 inv_transform = transpose(mat3x4(texelFetch(mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * mi_index + 1)),
                                                              texelFetch(mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * mi_index + 2)),
                                                              texelFetch(mesh_instances, int(MESH_INSTANCE_BUF_STRIDE * mi_index + 3))));

                const vec3 ro = (inv_transform * vec4(orig_ro, 1.0)).xyz;
                const vec3 rd = (inv_transform * vec4(orig_rd, 0.0)).xyz;
                const vec3 inv_d = safe_invert(rd);

                Traverse_BLAS_WithStack(blas_nodes, vtx_positions, vtx_indices, prim_indices, ro, rd, inv_d, int(mi_index), mi_node_index, mi_vindex.x,
                                        stack_size, inter, mi_geo_index_count);
            }

            leaf_node = cur;
            if ((cur & BVH2_PRIM_COUNT_BITS) != 0) {
                cur = g_stack[gl_LocalInvocationIndex][--stack_size];
            }
        }
    }
}

#endif // SWRT_COMMON_GLSL