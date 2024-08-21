#ifndef LIGHT_BVH_COMMON_GLSL
#define LIGHT_BVH_COMMON_GLSL
#extension GL_EXT_control_flow_attributes : enable

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

vec3 decode_oct_dir(const uint oct) {
    vec3 ret;
    ret.x = -1.0 + 2.0 * float((oct >> 16) & 0x0000ffff) / 65535.0;
    ret.y = -1.0 + 2.0 * float(oct & 0x0000ffff) / 65535.0;
    ret.z = 1.0 - abs(ret.x) - abs(ret.y);
    if (ret.z < 0.0) {
        const float temp = ret.x;
        ret.x = (1.0 - abs(ret.y)) * copysign(1.0, temp);
        ret.y = (1.0 - abs(temp)) * copysign(1.0, ret.y);
    }
    return normalize(ret);
}

vec2 decode_cosines(const uint val) {
    const uvec2 ab = uvec2((val >> 16) & 0x0000ffff, (val & 0x0000ffff));
    return 2.0 * (vec2(ab) / 65535.0) - 1.0;
}

float calc_lnode_importance(const light_wbvh_node_t n, const vec3 P, out float importance[8]) {
    float total_importance = 0.0;
    for (int i = 0; i < 8; ++i) {
        float mul = 1.0, v_len2 = 1.0;
        if (n.bbox_min[0][i] > -MAX_DIST) {
            const vec3 axis = decode_oct_dir(n.axis[i]);
            const vec3 ext = vec3(n.bbox_max[0][i] - n.bbox_min[0][i],
                                  n.bbox_max[1][i] - n.bbox_min[1][i],
                                  n.bbox_max[2][i] - n.bbox_min[2][i]);
            const float extent = 0.5 * length(ext);

            const vec3 pc = 0.5 * vec3(n.bbox_min[0][i] + n.bbox_max[0][i],
                                       n.bbox_min[1][i] + n.bbox_max[1][i],
                                       n.bbox_min[2][i] + n.bbox_max[2][i]);
            vec3 wi = P - pc;
            const float dist2 = dot(wi, wi);
            const float dist = sqrt(dist2);
            wi /= dist;

            v_len2 = max(dist2, extent);

            const float cos_omega_w = dot(axis, wi);
            const float sin_omega_w = sqrt(1.0 - cos_omega_w * cos_omega_w);

            float cos_omega_b = -1.0;
            if (dist2 >= extent * extent) {
                cos_omega_b = sqrt(1.0 - (extent * extent) / dist2);
            }
            const float sin_omega_b = sqrt(1.0 - cos_omega_b * cos_omega_b);

            const vec2 cos_omega_ne = decode_cosines(n.cos_omega_ne[i]);
            const float sin_omega_n = sqrt(1.0 - cos_omega_ne[0] * cos_omega_ne[0]);

            const float cos_omega_x = cos_sub_clamped(sin_omega_w, cos_omega_w, sin_omega_n, cos_omega_ne[0]);
            const float sin_omega_x = sin_sub_clamped(sin_omega_w, cos_omega_w, sin_omega_n, cos_omega_ne[0]);
            const float cos_omega = cos_sub_clamped(sin_omega_x, cos_omega_x, sin_omega_b, cos_omega_b);

            mul = cos_omega > cos_omega_ne[1] ? cos_omega : 0.0;
        }
        importance[i] = n.flux[i] * mul / v_len2;
        total_importance += importance[i];
    }
    return total_importance;
}

light_wbvh_node_t FetchLightNode(samplerBuffer nodes_buf, const int li) {
    light_wbvh_node_t ret;
    for (int i = 0; i < 3; ++i) {
        const vec4 data0 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + i * 2 + 0);
        ret.bbox_min[i][0] = data0.x;
        ret.bbox_min[i][1] = data0.y;
        ret.bbox_min[i][2] = data0.z;
        ret.bbox_min[i][3] = data0.w;
        const vec4 data1 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + i * 2 + 1);
        ret.bbox_min[i][4] = data1.x;
        ret.bbox_min[i][5] = data1.y;
        ret.bbox_min[i][6] = data1.z;
        ret.bbox_min[i][7] = data1.w;
    }
    for (int i = 0; i < 3; ++i) {
        const vec4 data2 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 6 + i * 2 + 0);
        ret.bbox_max[i][0] = data2.x;
        ret.bbox_max[i][1] = data2.y;
        ret.bbox_max[i][2] = data2.z;
        ret.bbox_max[i][3] = data2.w;
        const vec4 data3 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 6 + i * 2 + 1);
        ret.bbox_max[i][4] = data3.x;
        ret.bbox_max[i][5] = data3.y;
        ret.bbox_max[i][6] = data3.z;
        ret.bbox_max[i][7] = data3.w;
    }
    const uvec4 data4 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 12));
    ret.child[0] = data4.x;
    ret.child[1] = data4.y;
    ret.child[2] = data4.z;
    ret.child[3] = data4.w;
    const uvec4 data5 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 13));
    ret.child[4] = data5.x;
    ret.child[5] = data5.y;
    ret.child[6] = data5.z;
    ret.child[7] = data5.w;
    const vec4 data6 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 14);
    ret.flux[0] = data6.x;
    ret.flux[1] = data6.y;
    ret.flux[2] = data6.z;
    ret.flux[3] = data6.w;
    const vec4 data7 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 15);
    ret.flux[4] = data7.x;
    ret.flux[5] = data7.y;
    ret.flux[6] = data7.z;
    ret.flux[7] = data7.w;
    const uvec4 data8 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 16));
    ret.axis[0] = data8.x;
    ret.axis[1] = data8.y;
    ret.axis[2] = data8.z;
    ret.axis[3] = data8.w;
    const uvec4 data9 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 17));
    ret.axis[4] = data9.x;
    ret.axis[5] = data9.y;
    ret.axis[6] = data9.z;
    ret.axis[7] = data9.w;
    const uvec4 data10 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 18));
    ret.cos_omega_ne[0] = data10.x;
    ret.cos_omega_ne[1] = data10.y;
    ret.cos_omega_ne[2] = data10.z;
    ret.cos_omega_ne[3] = data10.w;
    const uvec4 data11 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 19));
    ret.cos_omega_ne[4] = data11.x;
    ret.cos_omega_ne[5] = data11.y;
    ret.cos_omega_ne[6] = data11.z;
    ret.cos_omega_ne[7] = data11.w;
    return ret;
}

int PickLightSource(const vec3 P, samplerBuffer nodes_buf, const uint lights_count, float light_pick_rand, out float pdf_factor) {
#if USE_HIERARCHICAL_NEE
    pdf_factor = 1.0;
    light_wbvh_node_t n = FetchLightNode(nodes_buf, 0); // start from root
    while ((n.child[0] & LEAF_NODE_BIT) == 0) {
        float importance[8];
        const float total_importance = calc_lnode_importance(n, P, importance);
        [[dont_flatten]] if (total_importance == 0.0) {
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
        n = FetchLightNode(nodes_buf, int(n.child[next]));
        pdf_factor *= importance[next];
    }
    const int li = int(n.child[0] & PRIM_INDEX_BITS);
#else // USE_HIERARCHICAL_NEE
    const int li = clamp(int(light_pick_rand * lights_count), 0, int(lights_count - 1));
    pdf_factor = 1.0 / float(lights_count);
#endif // USE_HIERARCHICAL_NEE
    return li;
}

shared float g_stack_factors[64][48];

float EvalTriLightFactor(const vec3 P, samplerBuffer nodes_buf, samplerBuffer lights_buf, const uint lights_count, const uint tri_index, const vec3 ro) {
    uint stack_size = 0;
    g_stack_factors[gl_LocalInvocationIndex][stack_size] = 1.0;
    g_stack[gl_LocalInvocationIndex][stack_size++] = 0;

#if USE_HIERARCHICAL_NEE
    while (stack_size != 0) {
        const uint cur = g_stack[gl_LocalInvocationIndex][--stack_size];
        const float cur_factor = g_stack_factors[gl_LocalInvocationIndex][stack_size];

        light_wbvh_node_t n = FetchLightNode(nodes_buf, int(cur));

        if ((n.child[0] & LEAF_NODE_BIT) == 0) {
            float importance[8];
            const float total_importance = calc_lnode_importance(n, ro, importance);

            for (int j = 0; j < 8; ++j) {
                if (importance[j] > 0.0 && bbox_test(P, vec3(n.bbox_min[0][j], n.bbox_min[1][j], n.bbox_min[2][j]),
                                                        vec3(n.bbox_max[0][j], n.bbox_max[1][j], n.bbox_max[2][j]))) {
                    g_stack_factors[gl_LocalInvocationIndex][stack_size] = cur_factor * importance[j] / total_importance;
                    g_stack[gl_LocalInvocationIndex][stack_size++] = n.child[j];
                }
            }
        } else {
            const int light_index = int(n.child[0] & PRIM_INDEX_BITS);

            const light_item_t litem = FetchLightItem(lights_buf, light_index);
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

#endif // LIGHT_BVH_GLSL