#ifndef LIGHT_BVH_COMMON_GLSL
#define LIGHT_BVH_COMMON_GLSL
#extension GL_EXT_control_flow_attributes : enable

#include "principled_common.glsl" // for _light_item_t
#include "swrt_common.glsl"

#define USE_HIERARCHICAL_NEE 1

float cos_sub_clamped(const float sin_omega_a, const float cos_omega_a, const float sin_omega_b, const float cos_omega_b) {
    if (cos_omega_a > cos_omega_b) {
        return 1.0;
    }
    return cos_omega_a * cos_omega_b + sin_omega_a * sin_omega_b;
}

float sin_sub_clamped(const float sin_omega_a, const float cos_omega_a, const float sin_omega_b, const float cos_omega_b) {
    if (cos_omega_a > cos_omega_b) {
        return 0.0;
    }
    return sin_omega_a * cos_omega_b - cos_omega_a * sin_omega_b;
}

// 8-bit octahedral packing (compact light tree)
vec3 decode_oct_dir8(const uint oct) {
    vec3 ret;
    ret.x = -1.0 + 2.0 * float((oct >> 8) & 0xff) / 255.0;
    ret.y = -1.0 + 2.0 * float(oct & 0xff) / 255.0;
    ret.z = 1.0 - abs(ret.x) - abs(ret.y);
    if (ret.z < 0.0) {
        const float temp = ret.x;
        ret.x = (1.0 - abs(ret.y)) * copysign(1.0, temp);
        ret.y = (1.0 - abs(temp)) * copysign(1.0, ret.y);
    }
    return normalize(ret);
}

vec2 decode_cosines8(const uint val) {
    const uvec2 ab = uvec2((val >> 8) & 0xff, (val & 0xff));
    return 2.0 * (vec2(ab) / 254.0) - 1.0;
}

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
uint cwbvh_meta(const light_cwbvh_node_t n, const uint i) {
    return (i < 4u ? n.meta4_0 : n.meta4_1) >> (8u * (i & 3u)) & 0xffu;
}

// Internal children are stored contiguously - slot i resolves to child_base + popc(internal slots below i)
uint cwbvh_internal_child(const light_cwbvh_node_t n, const uint i) {
    const uint imask = n.child_base_imask >> 24u;
    return (n.child_base_imask & 0x00ffffffu) + uint(bitCount(imask & ~(0xffu << i)));
}

// Packed per-child accessors of the compact light node (see Types::light_cwbvh_node_t)
uint light_flux_at(const light_cwbvh_node_t n, const uint i) {
    return (n.flux[i >> 2u] >> (8u * (i & 3u))) & 0xffu;
}

// 8-bit fraction of the parent's flux, cube-coded. The per-node scale cancels: every consumer
// normalizes importance inside the node.
float light_flux_weight(const light_cwbvh_node_t n, const uint i) {
    const float v = float(light_flux_at(n, i)) * (1.0 / 255.0);
    return v * v * v;
}

uint light_axis_at(const light_cwbvh_node_t n, const uint i) {
    return (n.axis[i >> 1u] >> (16u * (i & 1u))) & 0xffffu;
}

uint light_cos_at(const light_cwbvh_node_t n, const uint i) {
    return (n.cos_omega_ne[i >> 1u] >> (16u * (i & 1u))) & 0xffffu;
}

float calc_lnode_importance(const light_cwbvh_node_t n, const vec3 P, out float importance[8]) {
    // pow2 quantization step stored in ch_exp (see pow2Extents() on the CPU side)
    const vec3 step = vec3(cwbvh_step(n.ch_exp, 0u), cwbvh_step(n.ch_exp, 1u), cwbvh_step(n.ch_exp, 2u));

    float total_importance = 0.0;
    for (uint i = 0; i < 8; ++i) {
        importance[i] = light_flux_weight(n, i);
        if (((n.ch_bbox_min[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) != 0xff ||
            ((n.ch_bbox_max[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) != 0) {
            const vec3 bbox_min = vec3(n.bbox_min[0] + float((n.ch_bbox_min[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[0],
                                       n.bbox_min[1] + float((n.ch_bbox_min[1][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[1],
                                       n.bbox_min[2] + float((n.ch_bbox_min[2][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[2]);
            const vec3 bbox_max = vec3(n.bbox_min[0] + float((n.ch_bbox_max[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[0],
                                       n.bbox_min[1] + float((n.ch_bbox_max[1][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[1],
                                       n.bbox_min[2] + float((n.ch_bbox_max[2][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[2]);
            const float extent = 0.5 * length(bbox_max - bbox_min);

            const vec3 pc = 0.5 * (bbox_min + bbox_max);
            vec3 wi = P - pc;
            const float dist2 = dot(wi, wi);
            const float dist = sqrt(dist2);
            wi /= dist;

            const float v_len2 = max(dist2, extent);

            const vec3 axis = decode_oct_dir8(light_axis_at(n, i));
            const float cos_omega_w = dot(axis, wi);
            const float sin_omega_w = sqrt(1.0 - cos_omega_w * cos_omega_w);

            float cos_omega_b = -1.0;
            if (dist2 >= extent * extent) {
                cos_omega_b = sqrt(1.0 - (extent * extent) / dist2);
            }
            const float sin_omega_b = sqrt(1.0 - cos_omega_b * cos_omega_b);

            const vec2 cos_omega_ne = decode_cosines8(light_cos_at(n, i));
            const float sin_omega_n = sqrt(1.0 - cos_omega_ne[0] * cos_omega_ne[0]);

            const float cos_omega_x = cos_sub_clamped(sin_omega_w, cos_omega_w, sin_omega_n, cos_omega_ne[0]);
            const float sin_omega_x = sin_sub_clamped(sin_omega_w, cos_omega_w, sin_omega_n, cos_omega_ne[0]);
            const float cos_omega = cos_sub_clamped(sin_omega_x, cos_omega_x, sin_omega_b, cos_omega_b);

            importance[i] *= (cos_omega > cos_omega_ne[1]) ? (cos_omega / v_len2) : 0.0;
        }
        total_importance += importance[i];
    }
    return total_importance;
}

float calc_lnode_importance(const light_cwbvh_node_t n, const vec3 P, const vec3 P_test, out float importance[8]) {
    // pow2 quantization step stored in ch_exp (see pow2Extents() on the CPU side)
    const vec3 step = vec3(cwbvh_step(n.ch_exp, 0u), cwbvh_step(n.ch_exp, 1u), cwbvh_step(n.ch_exp, 2u));

    float total_importance = 0.0;
    for (uint i = 0; i < 8; ++i) {
        float imp = light_flux_weight(n, i);
        importance[i] = imp;
        if (((n.ch_bbox_min[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) != 0xff ||
            ((n.ch_bbox_max[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) != 0) {
            const vec3 bbox_min = vec3(n.bbox_min[0] + float((n.ch_bbox_min[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[0],
                                       n.bbox_min[1] + float((n.ch_bbox_min[1][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[1],
                                       n.bbox_min[2] + float((n.ch_bbox_min[2][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[2]);
            const vec3 bbox_max = vec3(n.bbox_min[0] + float((n.ch_bbox_max[0][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[0],
                                       n.bbox_min[1] + float((n.ch_bbox_max[1][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[1],
                                       n.bbox_min[2] + float((n.ch_bbox_max[2][i / 4u] >> (8u * (i % 4u))) & 0xffu) * step[2]);
            const float extent = 0.5 * length(bbox_max - bbox_min);

            const vec3 pc = 0.5 * (bbox_min + bbox_max);
            vec3 wi = P - pc;
            const float dist2 = dot(wi, wi);
            const float dist = sqrt(dist2);
            wi /= dist;

            const float v_len2 = max(dist2, extent);

            const vec3 axis = decode_oct_dir8(light_axis_at(n, i));
            const float cos_omega_w = dot(axis, wi);
            const float sin_omega_w = sqrt(1.0 - cos_omega_w * cos_omega_w);

            float cos_omega_b = -1.0;
            if (dist2 >= extent * extent) {
                cos_omega_b = sqrt(1.0 - (extent * extent) / dist2);
            }
            const float sin_omega_b = sqrt(1.0 - cos_omega_b * cos_omega_b);

            const vec2 cos_omega_ne = decode_cosines8(light_cos_at(n, i));
            const float sin_omega_n = sqrt(1.0 - cos_omega_ne[0] * cos_omega_ne[0]);

            const float cos_omega_x = cos_sub_clamped(sin_omega_w, cos_omega_w, sin_omega_n, cos_omega_ne[0]);
            const float sin_omega_x = sin_sub_clamped(sin_omega_w, cos_omega_w, sin_omega_n, cos_omega_ne[0]);
            const float cos_omega = cos_sub_clamped(sin_omega_x, cos_omega_x, sin_omega_b, cos_omega_b);

            const float mul = (cos_omega > cos_omega_ne[1]) ? (cos_omega / v_len2) : 0.0;
            importance[i] = imp = (imp * mul);
            if (!bbox_test(P_test, bbox_min, bbox_max)) {
                // NOTE: total_importance must not account for this!
                importance[i] = 0.0;
            }
        }
        total_importance += imp;
    }
    return total_importance;
}

light_cwbvh_node_t FetchLightCWBVHNode(samplerBuffer nodes_buf, const int li) {
    light_cwbvh_node_t ret;
    const uvec4 data0 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 0));
    ret.bbox_min[0] = uintBitsToFloat(data0.x);
    ret.bbox_min[1] = uintBitsToFloat(data0.y);
    ret.bbox_min[2] = uintBitsToFloat(data0.z);
    ret.ch_bbox_min[0][0] = data0.w;
    const uvec4 data1 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 1));
    ret.ch_bbox_min[0][1] = data1.x;
    ret.ch_bbox_min[1][0] = data1.y;
    ret.ch_bbox_min[1][1] = data1.z;
    ret.ch_bbox_min[2][0] = data1.w;
    const uvec4 data2 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 2));
    ret.ch_bbox_min[2][1] = data2.x;
    ret.ch_bbox_max[0][0] = data2.y;
    ret.ch_bbox_max[0][1] = data2.z;
    ret.ch_bbox_max[1][0] = data2.w;
    const uvec4 data3 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 3));
    ret.ch_bbox_max[1][1] = data3.x;
    ret.ch_bbox_max[2][0] = data3.y;
    ret.ch_bbox_max[2][1] = data3.z;
    ret.ch_exp = data3.w;
    const uvec4 data4 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 4));
    ret.child_base_imask = data4.x;
    ret.prim_base = data4.y;
    ret.meta4_0 = data4.z;
    ret.meta4_1 = data4.w;
    const uvec4 data5 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 5));
    ret.ch_bitmask[0] = data5.x;
    ret.ch_bitmask[1] = data5.y;
    ret.flux[0] = data5.z;
    ret.flux[1] = data5.w;
    const uvec4 data6 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 6));
    ret.axis[0] = data6.x;
    ret.axis[1] = data6.y;
    ret.axis[2] = data6.z;
    ret.axis[3] = data6.w;
    const uvec4 data7 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 7));
    ret.cos_omega_ne[0] = data7.x;
    ret.cos_omega_ne[1] = data7.y;
    ret.cos_omega_ne[2] = data7.z;
    ret.cos_omega_ne[3] = data7.w;
    return ret;
}

int PickLightSource(const vec3 P, samplerBuffer nodes_buf, usamplerBuffer leaf_prims_buf, const uint lights_count,
                    float light_pick_rand, out float pdf_factor) {
#if USE_HIERARCHICAL_NEE
    pdf_factor = 1.0;

    uint cur = 0; // start from root
    while ((cur & LEAF_NODE_BIT) == 0) {
        const light_cwbvh_node_t n = FetchLightCWBVHNode(nodes_buf, int(cur));

        float importance[8];
        const float total_importance = calc_lnode_importance(n, P, importance);
        if (total_importance == 0.0) {
            // failed to find lightsource for sampling
            return -1;
        }

        // normalize
        [[unroll]] for (int j = 0; j < 8; ++j) {
            importance[j] /= total_importance;
        }

        float importance_cdf[9];
        importance_cdf[0] = 0.0;
        [[unroll]] for (int j = 0; j < 8; ++j) {
            importance_cdf[j + 1] = importance_cdf[j] + importance[j];
        }
        // make sure cdf ends with 1.0
        [[unroll]] for (int j = 0; j < 8; ++j) {
            [[flatten]] if (importance_cdf[j + 1] == importance_cdf[8]) {
                importance_cdf[j + 1] = 1.01;
            }
        }

        int next = 0;
        [[unroll]] for (int j = 1; j < 9; ++j) {
            if (importance_cdf[j] <= light_pick_rand) {
                ++next;
            }
        }

        light_pick_rand = fract((light_pick_rand - importance_cdf[next]) / importance[next]);
        pdf_factor *= importance[next];

        // Implicit child addressing + meta4 leaf payloads (CWBVH layout): the 0x40 meta bit marks
        // a leaf child resolved through the parent's contiguous leaf prims block
        const uint meta = cwbvh_meta(n, uint(next));
        if ((meta & 0x40u) != 0u) {
            cur = texelFetch(leaf_prims_buf, int(n.prim_base + (meta & 0x3fu))).x;
        } else {
            cur = cwbvh_internal_child(n, uint(next));
        }
    }
    return int(cur & PRIM_INDEX_BITS);
#else // USE_HIERARCHICAL_NEE
    pdf_factor = 1.0 / float(lights_count);
    return clamp(int(light_pick_rand * lights_count), 0, int(lights_count - 1));
#endif // USE_HIERARCHICAL_NEE
}

#ifndef MAX_STACK_FACTORS_SIZE
    #define MAX_STACK_FACTORS_SIZE MAX_STACK_SIZE
#endif

shared float g_stack_factors[64][MAX_STACK_SIZE];

float EvalTriLightFactor(const vec3 P, samplerBuffer nodes_buf, usamplerBuffer leaf_prims_buf,
                         usamplerBuffer lights_buf, const uint lights_count, const uint tri_index, const vec3 ro) {
    uint stack_size = 0;
    g_stack_factors[gl_LocalInvocationIndex][stack_size] = 1.0;
    g_stack[gl_LocalInvocationIndex][stack_size++] = 0;

#if USE_HIERARCHICAL_NEE
    while (stack_size != 0) {
        const uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];
        const float cur_factor = g_stack_factors[gl_LocalInvocationIndex][stack_size];

        if ((cur & LEAF_NODE_BIT) == 0) {
            const light_cwbvh_node_t n = FetchLightCWBVHNode(nodes_buf, int(cur));

            float importance[8];
            const float total_importance = calc_lnode_importance(n, ro, P, importance);

            for (int j = 0; j < 8; ++j) {
                if (importance[j] > 0.0) {
                    g_stack_factors[gl_LocalInvocationIndex][stack_size] = cur_factor * importance[j] / total_importance;
                    const uint meta = cwbvh_meta(n, uint(j));
                    if ((meta & 0x40u) != 0u) {
                        g_stack[gl_LocalInvocationIndex][stack_size++] =
                            texelFetch(leaf_prims_buf, int(n.prim_base + (meta & 0x3fu))).x;
                    } else {
                        g_stack[gl_LocalInvocationIndex][stack_size++] = cwbvh_internal_child(n, uint(j));
                    }
                }
            }
        } else {
            const uint light_index = (cur & PRIM_INDEX_BITS);

            const _light_item_t litem = FetchLightItem(lights_buf, light_index);
            const uint type = floatBitsToUint(litem.col_and_type.w) & LIGHT_TYPE_BITS;
            if (type == LIGHT_TYPE_TRI && floatBitsToUint(litem.shadow_pos_and_tri_index.w) == tri_index) {
                return cur_factor;
            }
        }
    }
    return 1.0;
#else // USE_HIERARCHICAL_NEE
    return 1.0 / float(lights_count);
#endif // USE_HIERARCHICAL_NEE
}

#endif // LIGHT_BVH_COMMON_GLSL
