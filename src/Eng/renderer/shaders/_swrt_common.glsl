
#include "_cs_common.glsl"

#define MAX_DIST 3.402823466e+30

#define FLT_EPS 0.0000001
#define FLT_MAX 3.402823466e+38

bool IntersectTri(vec3 o, vec3 d, vec3 v0, vec3 v1, vec3 v2, out float t, out float u, out float v) {
    vec3 e10 = v1 - v0;
    vec3 e20 = v2 - v0;

    vec3 pvec = cross(d, e20);
    float det = dot(e10, pvec);
    if (det < FLT_EPS) {
        return false;
    }

    vec3 tvec = o - v0;
    u = dot(tvec, pvec);
    if (u < 0.0 || u > det) {
        return false;
    }

    vec3 qvec = cross(tvec, e10);
    v = dot(d, qvec);
    if (v < 0.0 || u + v > det) {
        return false;
    }

    t = dot(e20, qvec);

    float inv_det = 1.0 / det;

    t *= inv_det;
    u *= inv_det;
    v *= inv_det;

    return true;
}

bool _bbox_test_fma(vec3 inv_d, vec3 neg_inv_d_o, float t, vec3 bbox_min, vec3 bbox_max) {
    float low = fma(inv_d.x, bbox_min.x, neg_inv_d_o.x);
    float high = fma(inv_d.x, bbox_max.x, neg_inv_d_o.x);
    float tmin = min(low, high);
    float tmax = max(low, high);

    low = fma(inv_d.y, bbox_min.y, neg_inv_d_o.y);
    high = fma(inv_d.y, bbox_max.y, neg_inv_d_o.y);
    tmin = max(tmin, min(low, high));
    tmax = min(tmax, max(low, high));

    low = fma(inv_d.z, bbox_min.z, neg_inv_d_o.z);
    high = fma(inv_d.z, bbox_max.z, neg_inv_d_o.z);
    tmin = max(tmin, min(low, high));
    tmax = min(tmax, max(low, high));
    tmax *= 1.00000024;

    return tmin <= tmax && tmin <= t && tmax > 0.0;
}

vec3 safe_invert(vec3 v) {
    vec3 inv_v = 1.0f / v;

    if (v.x <= FLT_EPS && v.x >= 0) {
        inv_v.x = FLT_MAX;
    } else if (v.x >= -FLT_EPS && v.x < 0) {
        inv_v.x = -FLT_MAX;
    }

    if (v.y <= FLT_EPS && v.y >= 0) {
        inv_v.y = FLT_MAX;
    } else if (v.y >= -FLT_EPS && v.y < 0) {
        inv_v.y = -FLT_MAX;
    }

    if (v.z <= FLT_EPS && v.z >= 0) {
        inv_v.z = FLT_MAX;
    } else if (v.z >= -FLT_EPS && v.z < 0) {
        inv_v.z = -FLT_MAX;
    }

    return inv_v;
}

const uint LEAF_NODE_BIT = (1u << 31);
const uint PRIM_INDEX_BITS = ~LEAF_NODE_BIT;
const uint LEFT_CHILD_BITS = ~LEAF_NODE_BIT;

const uint SEP_AXIS_BITS = (3u << 30); // 0b11u
const uint PRIM_COUNT_BITS = ~SEP_AXIS_BITS;
const uint RIGHT_CHILD_BITS = ~SEP_AXIS_BITS;

struct bvh_node_t {
    vec4 bbox_min; // w is prim_index/left_child
    vec4 bbox_max; // w is prim_count/right_child
};

struct mesh_t {
    uint node_index, node_count;
    uint tris_index, tris_count;
    uint vert_index, geo_count;
};

struct mesh_instance_t {
    vec4 bbox_min; // w is geo_index
    vec4 bbox_max; // w is mesh_index
    mat3x4 inv_transform;
};

struct hit_data_t {
    int mask;
    int obj_index;
    int geo_index;
    int geo_count;
    int prim_index;
    float t, u, v;
};

#define near_child(rd, n)   \
    (rd)[floatBitsToUint(n.bbox_max.w) >> 30] < 0 ? (floatBitsToUint(n.bbox_max.w) & RIGHT_CHILD_BITS) : floatBitsToUint(n.bbox_min.w)

#define far_child(rd, n)    \
    (rd)[floatBitsToUint(n.bbox_max.w) >> 30] < 0 ? floatBitsToUint(n.bbox_min.w) : (floatBitsToUint(n.bbox_max.w) & RIGHT_CHILD_BITS)

void IntersectTris_ClosestHit(samplerBuffer vtx_positions, usamplerBuffer vtx_indices, usamplerBuffer prim_indices,
                              vec3 ro, vec3 rd, int tri_start, int tri_end, uint first_vertex,
                              int obj_index, inout hit_data_t out_inter, int geo_index, int geo_count) {
    for (int i = tri_start; i < tri_end; ++i) {
        int j = int(texelFetch(prim_indices, i).x);

        uint i0 = texelFetch(vtx_indices, 3 * j + 0).x;
        uint i1 = texelFetch(vtx_indices, 3 * j + 1).x;
        uint i2 = texelFetch(vtx_indices, 3 * j + 2).x;

        float t, u, v;
        if (IntersectTri(ro, rd, texelFetch(vtx_positions, int(first_vertex + i0)).xyz,
                                 texelFetch(vtx_positions, int(first_vertex + i1)).xyz,
                                 texelFetch(vtx_positions, int(first_vertex + i2)).xyz, t, u, v) && t > 0.0 && t < out_inter.t) {
            out_inter.mask = -1;
            out_inter.prim_index = int(j);
            out_inter.obj_index = obj_index;
            out_inter.geo_index = geo_index;
            out_inter.geo_count = geo_count;
            out_inter.t = t;
            out_inter.u = u;
            out_inter.v = v;
        }
    }
}

const int MAX_STACK_SIZE = 48;
shared uint g_stack[64][MAX_STACK_SIZE];

void Traverse_MicroTree_WithStack(samplerBuffer blas_nodes, samplerBuffer vtx_positions, usamplerBuffer vtx_indices, usamplerBuffer prim_indices,
                                  vec3 ro, vec3 rd, vec3 inv_d, int obj_index, uint node_index,
                                  uint first_vertex, uint stack_size, inout hit_data_t inter, int geo_index, int geo_count) {
    vec3 neg_inv_do = -inv_d * ro;

    uint initial_stack_size = stack_size;
    g_stack[gl_LocalInvocationIndex][stack_size++] = node_index;

    while (stack_size != initial_stack_size) {
        uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];

        vec4 bbox_min = texelFetch(blas_nodes, int(2 * cur + 0));
        vec4 bbox_max = texelFetch(blas_nodes, int(2 * cur + 1));

        if (!_bbox_test_fma(inv_d, neg_inv_do, inter.t, bbox_min.xyz, bbox_max.xyz)) {
            continue;
        }

        if ((floatBitsToUint(bbox_min.w) & LEAF_NODE_BIT) == 0) {
            uint left_child = floatBitsToUint(bbox_min.w);
            uint right_child = (floatBitsToUint(bbox_max.w) & RIGHT_CHILD_BITS);
            if (rd[floatBitsToUint(bbox_max.w) >> 30] < 0) {
                g_stack[gl_LocalInvocationIndex][stack_size++] = left_child;
                g_stack[gl_LocalInvocationIndex][stack_size++] = right_child;
            } else {
                g_stack[gl_LocalInvocationIndex][stack_size++] = right_child;
                g_stack[gl_LocalInvocationIndex][stack_size++] = left_child;
            }
        } else {
            int tri_beg = int(floatBitsToUint(bbox_min.w) & PRIM_INDEX_BITS);
            int tri_end = tri_beg + floatBitsToInt(bbox_max.w);

            IntersectTris_ClosestHit(vtx_positions, vtx_indices, prim_indices, ro, rd, tri_beg, tri_end, first_vertex, obj_index, inter, geo_index, geo_count);
        }
    }
}

void Traverse_MacroTree_WithStack(samplerBuffer tlas_nodes, samplerBuffer blas_nodes, samplerBuffer mesh_instances, usamplerBuffer meshes, samplerBuffer vtx_positions,
                                  usamplerBuffer vtx_indices, usamplerBuffer prim_indices, vec3 orig_ro, vec3 orig_rd, vec3 orig_inv_rd, uint node_index, inout hit_data_t inter) {
    vec3 orig_neg_inv_do = -orig_inv_rd * orig_ro;

    uint stack_size = 0;
    g_stack[gl_LocalInvocationIndex][stack_size++] = node_index;

    while (stack_size != 0) {
        uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];

        vec4 n_bbox_min = texelFetch(tlas_nodes, int(2 * cur + 0));
        vec4 n_bbox_max = texelFetch(tlas_nodes, int(2 * cur + 1));

        if (!_bbox_test_fma(orig_inv_rd, orig_neg_inv_do, inter.t, n_bbox_min.xyz, n_bbox_max.xyz)) {
            continue;
        }

        if ((floatBitsToUint(n_bbox_min.w) & LEAF_NODE_BIT) == 0) {
            uint left_child = floatBitsToUint(n_bbox_min.w);
            uint right_child = (floatBitsToUint(n_bbox_max.w) & RIGHT_CHILD_BITS);
            if (orig_rd[floatBitsToUint(n_bbox_max.w) >> 30] < 0) {
                g_stack[gl_LocalInvocationIndex][stack_size++] = left_child;
                g_stack[gl_LocalInvocationIndex][stack_size++] = right_child;
            } else {
                g_stack[gl_LocalInvocationIndex][stack_size++] = right_child;
                g_stack[gl_LocalInvocationIndex][stack_size++] = left_child;
            }
        } else {
            uint prim_index = (floatBitsToUint(n_bbox_min.w) & PRIM_INDEX_BITS);
            uint prim_count = floatBitsToUint(n_bbox_max.w);
            for (uint i = prim_index; i < prim_index + prim_count; ++i) {
                vec4 mi_bbox_min = texelFetch(mesh_instances, int(5 * i + 0));
                vec4 mi_bbox_max = texelFetch(mesh_instances, int(5 * i + 1));

                if (!_bbox_test_fma(orig_inv_rd, orig_neg_inv_do, inter.t, mi_bbox_min.xyz, mi_bbox_max.xyz)) {
                    continue;
                }

                mat4x3 inv_transform = transpose(mat3x4(texelFetch(mesh_instances, int(5 * i + 2)),
                                                        texelFetch(mesh_instances, int(5 * i + 3)),
                                                        texelFetch(mesh_instances, int(5 * i + 4))));

                vec3 ro = (inv_transform * vec4(orig_ro, 1.0)).xyz;
                vec3 rd = (inv_transform * vec4(orig_rd, 0.0)).xyz;
                vec3 inv_d = safe_invert(rd);

                uint mesh_index = floatBitsToUint(mi_bbox_max.w);
                uint m_node_index = texelFetch(meshes, int(3 * mesh_index + 0)).x;
                uvec2 m_vindex_gcount = texelFetch(meshes, int(3 * mesh_index + 2)).xy;

                uint geo_index = floatBitsToUint(mi_bbox_min.w);
                Traverse_MicroTree_WithStack(blas_nodes, vtx_positions, vtx_indices, prim_indices, ro, rd, inv_d, int(i), m_node_index, m_vindex_gcount.x,
                                             stack_size, inter, int(geo_index), int(m_vindex_gcount.y));
            }
        }
    }
}

#undef EPS