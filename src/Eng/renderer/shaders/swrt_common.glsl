#ifndef SWRT_COMMON_GLSL
#define SWRT_COMMON_GLSL
#extension GL_EXT_control_flow_attributes : enable

#include "_cs_common.glsl"

#define FLT_MAX 3.402823466e+38

#define IGNORE_BACKFACING 0

#ifndef SWRT_SUBGROUP
    #define SWRT_SUBGROUP 0
#endif

#if SWRT_SUBGROUP
    #define SUBGROUP_ALL(c) subgroupAll(c)
    #define SUBGROUP_ANY(c) subgroupAny(c)
#else
    #define SUBGROUP_ALL(c) (c)
    #define SUBGROUP_ANY(c) (c)
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

//
// Compact (CWBVH-style) node helpers - mirror the CPU side (Renderer_Structs.h)
//

// Per-axis quantization step 2^ch_exp[axis] (CWBVH paper trick): sign-extend the stored int8
// exponent and place it in the float exponent field. ch_exp is always in [-126, 127].
float cwbvh_step(const uint ch_exp, const uint axis) {
    const uint b = (ch_exp >> (8u * axis)) & 0xffu;
    return uintBitsToFloat((((b ^ 0x80u) - 1u) & 0xffu) << 23u);
}

// meta4 byte of child slot i: 0 = empty slot, 0x40|rank = leaf child, 0x80|i = internal child
uint cwbvh_meta(const cwbvh_node_t n, const uint i) {
    return (i < 4u ? n.meta4_0 : n.meta4_1) >> (8u * (i & 3u)) & 0xffu;
}

// Internal children are stored contiguously - slot i resolves to child_base + popc(internal slots below i)
uint cwbvh_internal_child(const cwbvh_node_t n, const uint i) {
    const uint imask = n.child_base_imask >> 24u;
    return (n.child_base_imask & 0x00ffffffu) + uint(bitCount(imask & ~(0xffu << i)));
}

cwbvh_node_t FetchCWBVHNode(samplerBuffer nodes_buf, const int li) {
    cwbvh_node_t ret;
    const uvec4 data0 = floatBitsToUint(texelFetch(nodes_buf, int(li * int(CWBVH_NODE_BUF_STRIDE) + 0)));
    ret.bbox_min[0] = uintBitsToFloat(data0.x);
    ret.bbox_min[1] = uintBitsToFloat(data0.y);
    ret.bbox_min[2] = uintBitsToFloat(data0.z);
    ret.ch_bbox_min[0][0] = data0.w;
    const uvec4 data1 = floatBitsToUint(texelFetch(nodes_buf, int(li * int(CWBVH_NODE_BUF_STRIDE) + 1)));
    ret.ch_bbox_min[0][1] = data1.x;
    ret.ch_bbox_min[1][0] = data1.y;
    ret.ch_bbox_min[1][1] = data1.z;
    ret.ch_bbox_min[2][0] = data1.w;
    const uvec4 data2 = floatBitsToUint(texelFetch(nodes_buf, int(li * int(CWBVH_NODE_BUF_STRIDE) + 2)));
    ret.ch_bbox_min[2][1] = data2.x;
    ret.ch_bbox_max[0][0] = data2.y;
    ret.ch_bbox_max[0][1] = data2.z;
    ret.ch_bbox_max[1][0] = data2.w;
    const uvec4 data3 = floatBitsToUint(texelFetch(nodes_buf, int(li * int(CWBVH_NODE_BUF_STRIDE) + 3)));
    ret.ch_bbox_max[1][1] = data3.x;
    ret.ch_bbox_max[2][0] = data3.y;
    ret.ch_bbox_max[2][1] = data3.z;
    ret.ch_exp = data3.w;
    const uvec4 data4 = floatBitsToUint(texelFetch(nodes_buf, int(li * int(CWBVH_NODE_BUF_STRIDE) + 4)));
    ret.child_base_imask = data4.x;
    ret.prim_base = data4.y;
    ret.meta4_0 = data4.z;
    ret.meta4_1 = data4.w;
    return ret;
}

// The hit mask is in *direction key space* (bit i ^ flip_bits), so draining it with findMSB()
// visits the octant closest to the ray origin first.
uint cwbvh_child_hit_mask(const cwbvh_node_t n, vec3 inv_d, vec3 neg_inv_do, float tmin, float tmax,
                          const uint flip_bits) {
    const vec3 _step = vec3(cwbvh_step(n.ch_exp, 0u), cwbvh_step(n.ch_exp, 1u), cwbvh_step(n.ch_exp, 2u));

    uint mask = 0u;
    for (uint i = 0; i < 8; ++i) {
        // empty slots are marked by a zero meta4 byte; scene children always have finite boxes
        if (cwbvh_meta(n, i) == 0u) {
            continue;
        }
        const vec3 bbox_min = vec3(n.bbox_min[0] + float((n.ch_bbox_min[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) * _step[0],
                                   n.bbox_min[1] + float((n.ch_bbox_min[1][i / 4u] >> (8u * (i % 4u))) & 0xffu) * _step[1],
                                   n.bbox_min[2] + float((n.ch_bbox_min[2][i / 4u] >> (8u * (i % 4u))) & 0xffu) * _step[2]);
        const vec3 bbox_max = vec3(n.bbox_min[0] + float((n.ch_bbox_max[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) * _step[0],
                                   n.bbox_min[1] + float((n.ch_bbox_max[1][i / 4u] >> (8u * (i % 4u))) & 0xffu) * _step[1],
                                   n.bbox_min[2] + float((n.ch_bbox_max[2][i / 4u] >> (8u * (i % 4u))) & 0xffu) * _step[2]);

        if (bbox_test(inv_d, neg_inv_do, tmin, tmax, bbox_min, bbox_max)) {
            mask |= (1u << (i ^ flip_bits));
        }
    }
    return mask;
}

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
    #define MAX_STACK_SIZE 64
#endif

#ifndef SM_STACK_SIZE
    #if defined(VULKAN)
        #define SM_STACK_SIZE 10
    #else
        // Workaround for a crash on GL nvidia
        #define SM_STACK_SIZE 0
    #endif
#endif

#if SM_STACK_SIZE > 0
shared uint g_stack[64][SM_STACK_SIZE];
#endif

#if SM_STACK_SIZE >= MAX_STACK_SIZE
    #define STACK_PUSH(V)                                                                      \
        g_stack[gl_LocalInvocationIndex][stack_size++] = (V);

    #define STACK_POP(V)                                                                       \
        (V) = g_stack[gl_LocalInvocationIndex][--stack_size];
#elif SM_STACK_SIZE == 0
    uint g_stack_spill[MAX_STACK_SIZE]; // whole stack lives here (no shared array)

    #define STACK_PUSH(V)                                                                      \
        g_stack_spill[stack_size++] = (V);

    #define STACK_POP(V)                                                                       \
        (V) = g_stack_spill[--stack_size];
#else
    uint g_stack_spill[MAX_STACK_SIZE - SM_STACK_SIZE]; // expected to be placed into scratch memory

    #define STACK_PUSH(V)                                                                      \
        {                                                                                      \
            if (stack_size < SM_STACK_SIZE) {                                                  \
                g_stack[gl_LocalInvocationIndex][stack_size] = (V);                            \
            } else {                                                                           \
                g_stack_spill[stack_size - SM_STACK_SIZE] = (V);                               \
            }                                                                                  \
            ++stack_size;                                                                      \
        }

    #define STACK_POP(V)                                                                       \
        {                                                                                      \
            --stack_size;                                                                      \
            if (stack_size < SM_STACK_SIZE) {                                                  \
                (V) = g_stack[gl_LocalInvocationIndex][stack_size];                            \
            } else {                                                                           \
                (V) = g_stack_spill[stack_size - SM_STACK_SIZE];                               \
            }                                                                                  \
        }
#endif

// BLAS leaves reference 8-primitive blocks in prim_indices through blas_leaf_prims
// (payload = block base, baked with the mesh's prim indices offset)
void Traverse_BLAS_WithStack(samplerBuffer blas_nodes, usamplerBuffer blas_leaf_prims, samplerBuffer vtx_positions,
                             usamplerBuffer vtx_indices, usamplerBuffer prim_indices,
                             vec3 ro, vec3 rd, vec3 inv_d, int obj_index, uint node_index,
                             uint first_vertex, uint stack_size, inout hit_data_t inter, uint geo_index_count) {
    const vec3 neg_inv_do = -inv_d * ro;
    const uint flip_bits = (rd.x < 0.0 ? 1u : 0u) | (rd.y < 0.0 ? 2u : 0u) | (rd.z < 0.0 ? 4u : 0u);

    const uint initial_stack_size = stack_size;
    STACK_PUSH(node_index);

    uint index = 0u;
    while (true) {
        while (true) {
            if ((index & LEAF_NODE_BIT) == 0 && stack_size != initial_stack_size) {
                STACK_POP(index);
                [[flatten]] if ((index & LEAF_NODE_BIT) != 0) {
                    continue;
                }

                const cwbvh_node_t n = FetchCWBVHNode(blas_nodes, int(index));

                uint mask = cwbvh_child_hit_mask(n, inv_d, neg_inv_do, inter.tmin, inter.tmax, flip_bits);
                while (mask != 0u) {
                    const uint key = uint(findMSB(mask));
                    mask ^= 1u << key;

                    const uint i = key ^ flip_bits;
                    const uint meta = cwbvh_meta(n, i);
                    if ((meta & 0x80u) != 0u) {
                        STACK_PUSH(cwbvh_internal_child(n, i));
                    } else {
                        // leaf child: meta4 rank -> the parent's payload block (8-primitive block base)
                        const uint prim_start = (n.prim_base + (meta & 0x3fu)) | LEAF_NODE_BIT;
                        if ((index & LEAF_NODE_BIT) == 0) {
                            index = prim_start;
                        } else {
                            STACK_PUSH(prim_start);
                        }
                    }
                }
            }

            if (SUBGROUP_ALL((index & LEAF_NODE_BIT) != 0 || stack_size == initial_stack_size)) {
                break;
            }
        }

        if ((index & LEAF_NODE_BIT) != 0) {
            const int tri_start = int(texelFetch(blas_leaf_prims, int(index & PRIM_INDEX_BITS)).x);
            index = 0u;

            IntersectTris_ClosestHit(vtx_positions, vtx_indices, prim_indices, ro, rd,
                                     tri_start, tri_start + 8, first_vertex, obj_index, inter, geo_index_count);
        }

        if (SUBGROUP_ALL((index & LEAF_NODE_BIT) == 0 && stack_size == initial_stack_size)) {
            break;
        }
    }
}


// TLAS leaves reference mesh instances through tlas_leaf_prims
void Traverse_TLAS_WithStack(samplerBuffer tlas_nodes, usamplerBuffer tlas_leaf_prims, samplerBuffer blas_nodes,
                             usamplerBuffer blas_leaf_prims, samplerBuffer mesh_instances, samplerBuffer vtx_positions,
                             usamplerBuffer vtx_indices, usamplerBuffer prim_indices, vec3 orig_ro, vec3 orig_rd,
                             vec3 orig_inv_rd, uint ray_flags, uint node_index, inout hit_data_t inter) {
    const vec3 orig_neg_inv_do = -orig_inv_rd * orig_ro;
    const uint flip_bits = (orig_rd.x < 0.0 ? 1u : 0u) | (orig_rd.y < 0.0 ? 2u : 0u) | (orig_rd.z < 0.0 ? 4u : 0u);

    uint stack_size = 0;
    STACK_PUSH(node_index);

    uint index = 0u;
    while (true) {
        while (SUBGROUP_ANY((index & LEAF_NODE_BIT) == 0 && stack_size != 0u)) {
            if ((index & LEAF_NODE_BIT) == 0 && stack_size != 0u) {
                STACK_POP(index);
                [[flatten]] if ((index & LEAF_NODE_BIT) != 0) {
                    continue;
                }

                const cwbvh_node_t n = FetchCWBVHNode(tlas_nodes, int(index));

                uint mask = cwbvh_child_hit_mask(n, orig_inv_rd, orig_neg_inv_do, inter.tmin, inter.tmax, flip_bits);
                while (mask != 0u) {
                    const uint key = uint(findMSB(mask));
                    mask ^= 1u << key;

                    const uint i = key ^ flip_bits;
                    const uint meta = cwbvh_meta(n, i);
                    if ((meta & 0x80u) != 0u) {
                        STACK_PUSH(cwbvh_internal_child(n, i));
                    } else {
                        // leaf child: TLAS leaves are single mesh instances (meta4 rank -> payload)
                        STACK_PUSH((n.prim_base + (meta & 0x3fu)) | LEAF_NODE_BIT);
                    }
                }
            }
        }

        if ((index & LEAF_NODE_BIT) != 0) {
            const uint mi_index = texelFetch(tlas_leaf_prims, int(index & PRIM_INDEX_BITS)).x;
            index = 0u;

            const uvec4 mi_data = floatBitsToUint(texelFetch(mesh_instances, int(SWRT_MESH_INSTANCE_BUF_STRIDE * mi_index + 0)));
            const uint mi_visibility = mi_data.x, mi_node_index = mi_data.y, mi_vindex = mi_data.z, mi_geo_index_count = mi_data.w;
            if ((mi_visibility & ray_flags) != 0) {
                const mat4x3 inv_transform = transpose(mat3x4(texelFetch(mesh_instances, int(SWRT_MESH_INSTANCE_BUF_STRIDE * mi_index + 1)),
                                                              texelFetch(mesh_instances, int(SWRT_MESH_INSTANCE_BUF_STRIDE * mi_index + 2)),
                                                              texelFetch(mesh_instances, int(SWRT_MESH_INSTANCE_BUF_STRIDE * mi_index + 3))));

                const vec3 ro = (inv_transform * vec4(orig_ro, 1.0)).xyz;
                const vec3 rd = (inv_transform * vec4(orig_rd, 0.0)).xyz;
                const vec3 inv_d = safe_invert(rd);

                Traverse_BLAS_WithStack(blas_nodes, blas_leaf_prims, vtx_positions, vtx_indices, prim_indices, ro, rd, inv_d,
                                        int(mi_index), mi_node_index, mi_vindex.x, stack_size, inter, mi_geo_index_count);
            }
        }

        if (SUBGROUP_ALL(stack_size == 0u)) {
            break;
        }
    }
}

#endif // SWRT_COMMON_GLSL
