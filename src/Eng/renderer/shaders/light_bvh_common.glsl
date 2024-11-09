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
    return 2.0 * (vec2(ab) / 65534.0) - 1.0;
}

float calc_lnode_importance(const light_wbvh_node_t n, const vec3 P, out float importance[8]) {
    float total_importance = 0.0;
    for (int i = 0; i < 8; ++i) {
        importance[i] = n.flux[i];
        /*if (n.bbox_min[0][i] > -MAX_DIST)*/ {
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

            const float v_len2 = max(dist2, extent);

            const vec3 axis = decode_oct_dir(n.axis[i]);
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

            importance[i] *= (cos_omega > cos_omega_ne[1]) ? (cos_omega / v_len2) : 0.0;
        }
        total_importance += importance[i];
    }
    return total_importance;
}

float calc_lnode_importance(const light_cwbvh_node_t n, const vec3 P, out float importance[8]) {
    const vec3 decode_ext = vec3(n.bbox_max[0] - n.bbox_min[0],
                                 n.bbox_max[1] - n.bbox_min[1],
                                 n.bbox_max[2] - n.bbox_min[2]) / 255.0;

    float total_importance = 0.0;
    for (uint i = 0; i < 8; ++i) {
        importance[i] = n.flux[i];
        /*if (n.ch_bbox_min[0][i] != 0xff || n.ch_bbox_max[0][i] != 0)*/ {
            const vec3 bbox_min = vec3(n.bbox_min[0] + float((n.ch_bbox_min[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[0],
                                       n.bbox_min[1] + float((n.ch_bbox_min[1][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[1],
                                       n.bbox_min[2] + float((n.ch_bbox_min[2][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[2]);
            const vec3 bbox_max = vec3(n.bbox_min[0] + float((n.ch_bbox_max[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[0],
                                       n.bbox_min[1] + float((n.ch_bbox_max[1][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[1],
                                       n.bbox_min[2] + float((n.ch_bbox_max[2][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[2]);
            const float extent = 0.5 * length(bbox_max - bbox_min);

            const vec3 pc = 0.5 * (bbox_min + bbox_max);
            vec3 wi = P - pc;
            const float dist2 = dot(wi, wi);
            const float dist = sqrt(dist2);
            wi /= dist;

            const float v_len2 = max(dist2, extent);

            const vec3 axis = decode_oct_dir(n.axis[i]);
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

            importance[i] *= (cos_omega > cos_omega_ne[1]) ? (cos_omega / v_len2) : 0.0;
        }
        total_importance += importance[i];
    }
    return total_importance;
}

float calc_lnode_importance(const light_cwbvh_node_t n, const vec3 P, const vec3 P_test, out float importance[8]) {
    const vec3 decode_ext = vec3(n.bbox_max[0] - n.bbox_min[0],
                                 n.bbox_max[1] - n.bbox_min[1],
                                 n.bbox_max[2] - n.bbox_min[2]) / 255.0;

    float total_importance = 0.0;
    for (uint i = 0; i < 8; ++i) {
        float imp = n.flux[i];
        importance[i] = imp;
        /*if (n.ch_bbox_min[0][i] != 0xff || n.ch_bbox_max[0][i] != 0)*/ {
            const vec3 bbox_min = vec3(n.bbox_min[0] + float((n.ch_bbox_min[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[0],
                                       n.bbox_min[1] + float((n.ch_bbox_min[1][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[1],
                                       n.bbox_min[2] + float((n.ch_bbox_min[2][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[2]);
            const vec3 bbox_max = vec3(n.bbox_min[0] + float((n.ch_bbox_max[0][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[0],
                                       n.bbox_min[1] + float((n.ch_bbox_max[1][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[1],
                                       n.bbox_min[2] + float((n.ch_bbox_max[2][i / 4] >> (8u * (i % 4u))) & 0xffu) * decode_ext[2]);
            const float extent = 0.5 * length(bbox_max - bbox_min);

            const vec3 pc = 0.5 * (bbox_min + bbox_max);
            vec3 wi = P - pc;
            const float dist2 = dot(wi, wi);
            const float dist = sqrt(dist2);
            wi /= dist;

            const float v_len2 = max(dist2, extent);

            const vec3 axis = decode_oct_dir(n.axis[i]);
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

light_wbvh_node_t FetchLightWBVHNode(samplerBuffer nodes_buf, const int li) {
    light_wbvh_node_t ret;
    const vec3 bbox_min = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 0).xyz;
    const vec3 bbox_max = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 1).xyz;
    const vec3 ext = (bbox_max - bbox_min) / 255.0;
    { // unpack child bounds
        const uvec4 data2 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 2));
        ret.bbox_min[0][0] = bbox_min[0] + float((data2.x >> 0u) & 0xffu) * ext[0];
        ret.bbox_min[0][1] = bbox_min[0] + float((data2.x >> 8u) & 0xffu) * ext[0];
        ret.bbox_min[0][2] = bbox_min[0] + float((data2.x >> 16u) & 0xffu) * ext[0];
        ret.bbox_min[0][3] = bbox_min[0] + float((data2.x >> 24u) & 0xffu) * ext[0];

        ret.bbox_min[0][4] = bbox_min[0] + float((data2.y >> 0u) & 0xffu) * ext[0];
        ret.bbox_min[0][5] = bbox_min[0] + float((data2.y >> 8u) & 0xffu) * ext[0];
        ret.bbox_min[0][6] = bbox_min[0] + float((data2.y >> 16u) & 0xffu) * ext[0];
        ret.bbox_min[0][7] = bbox_min[0] + float((data2.y >> 24u) & 0xffu) * ext[0];

        ret.bbox_min[1][0] = bbox_min[1] + float((data2.z >> 0u) & 0xffu) * ext[1];
        ret.bbox_min[1][1] = bbox_min[1] + float((data2.z >> 8u) & 0xffu) * ext[1];
        ret.bbox_min[1][2] = bbox_min[1] + float((data2.z >> 16u) & 0xffu) * ext[1];
        ret.bbox_min[1][3] = bbox_min[1] + float((data2.z >> 24u) & 0xffu) * ext[1];

        ret.bbox_min[1][4] = bbox_min[1] + float((data2.w >> 0u) & 0xffu) * ext[1];
        ret.bbox_min[1][5] = bbox_min[1] + float((data2.w >> 8u) & 0xffu) * ext[1];
        ret.bbox_min[1][6] = bbox_min[1] + float((data2.w >> 16u) & 0xffu) * ext[1];
        ret.bbox_min[1][7] = bbox_min[1] + float((data2.w >> 24u) & 0xffu) * ext[1];

        const uvec4 data3 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 3));
        ret.bbox_min[2][0] = bbox_min[2] + float((data3.x >> 0u) & 0xffu) * ext[2];
        ret.bbox_min[2][1] = bbox_min[2] + float((data3.x >> 8u) & 0xffu) * ext[2];
        ret.bbox_min[2][2] = bbox_min[2] + float((data3.x >> 16u) & 0xffu) * ext[2];
        ret.bbox_min[2][3] = bbox_min[2] + float((data3.x >> 24u) & 0xffu) * ext[2];

        ret.bbox_min[2][4] = bbox_min[2] + float((data3.y >> 0u) & 0xffu) * ext[2];
        ret.bbox_min[2][5] = bbox_min[2] + float((data3.y >> 8u) & 0xffu) * ext[2];
        ret.bbox_min[2][6] = bbox_min[2] + float((data3.y >> 16u) & 0xffu) * ext[2];
        ret.bbox_min[2][7] = bbox_min[2] + float((data3.y >> 24u) & 0xffu) * ext[2];

        ret.bbox_max[0][0] = bbox_min[0] + float((data3.z >> 0u) & 0xffu) * ext[0];
        ret.bbox_max[0][1] = bbox_min[0] + float((data3.z >> 8u) & 0xffu) * ext[0];
        ret.bbox_max[0][2] = bbox_min[0] + float((data3.z >> 16u) & 0xffu) * ext[0];
        ret.bbox_max[0][3] = bbox_min[0] + float((data3.z >> 24u) & 0xffu) * ext[0];

        ret.bbox_max[0][4] = bbox_min[0] + float((data3.w >> 0u) & 0xffu) * ext[0];
        ret.bbox_max[0][5] = bbox_min[0] + float((data3.w >> 8u) & 0xffu) * ext[0];
        ret.bbox_max[0][6] = bbox_min[0] + float((data3.w >> 16u) & 0xffu) * ext[0];
        ret.bbox_max[0][7] = bbox_min[0] + float((data3.w >> 24u) & 0xffu) * ext[0];

        const uvec4 data4 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 4));
        ret.bbox_max[1][0] = bbox_min[1] + float((data4.x >> 0u) & 0xffu) * ext[1];
        ret.bbox_max[1][1] = bbox_min[1] + float((data4.x >> 8u) & 0xffu) * ext[1];
        ret.bbox_max[1][2] = bbox_min[1] + float((data4.x >> 16u) & 0xffu) * ext[1];
        ret.bbox_max[1][3] = bbox_min[1] + float((data4.x >> 24u) & 0xffu) * ext[1];

        ret.bbox_max[1][4] = bbox_min[1] + float((data4.y >> 0u) & 0xffu) * ext[1];
        ret.bbox_max[1][5] = bbox_min[1] + float((data4.y >> 8u) & 0xffu) * ext[1];
        ret.bbox_max[1][6] = bbox_min[1] + float((data4.y >> 16u) & 0xffu) * ext[1];
        ret.bbox_max[1][7] = bbox_min[1] + float((data4.y >> 24u) & 0xffu) * ext[1];

        ret.bbox_max[2][0] = bbox_min[2] + float((data4.z >> 0u) & 0xffu) * ext[2];
        ret.bbox_max[2][1] = bbox_min[2] + float((data4.z >> 8u) & 0xffu) * ext[2];
        ret.bbox_max[2][2] = bbox_min[2] + float((data4.z >> 16u) & 0xffu) * ext[2];
        ret.bbox_max[2][3] = bbox_min[2] + float((data4.z >> 24u) & 0xffu) * ext[2];

        ret.bbox_max[2][4] = bbox_min[2] + float((data4.w >> 0u) & 0xffu) * ext[2];
        ret.bbox_max[2][5] = bbox_min[2] + float((data4.w >> 8u) & 0xffu) * ext[2];
        ret.bbox_max[2][6] = bbox_min[2] + float((data4.w >> 16u) & 0xffu) * ext[2];
        ret.bbox_max[2][7] = bbox_min[2] + float((data4.w >> 24u) & 0xffu) * ext[2];
    }
    const uvec4 data5 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 5));
    ret.child[0] = data5.x;
    ret.child[1] = data5.y;
    ret.child[2] = data5.z;
    ret.child[3] = data5.w;
    const uvec4 data6 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 6));
    ret.child[4] = data6.x;
    ret.child[5] = data6.y;
    ret.child[6] = data6.z;
    ret.child[7] = data6.w;
    const vec4 data7 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 7);
    ret.flux[0] = data7.x;
    ret.flux[1] = data7.y;
    ret.flux[2] = data7.z;
    ret.flux[3] = data7.w;
    const vec4 data8 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 8);
    ret.flux[4] = data8.x;
    ret.flux[5] = data8.y;
    ret.flux[6] = data8.z;
    ret.flux[7] = data8.w;
    const uvec4 data9 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 9));
    ret.axis[0] = data9.x;
    ret.axis[1] = data9.y;
    ret.axis[2] = data9.z;
    ret.axis[3] = data9.w;
    const uvec4 data10 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 10));
    ret.axis[4] = data10.x;
    ret.axis[5] = data10.y;
    ret.axis[6] = data10.z;
    ret.axis[7] = data10.w;
    const uvec4 data11 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 11));
    ret.cos_omega_ne[0] = data11.x;
    ret.cos_omega_ne[1] = data11.y;
    ret.cos_omega_ne[2] = data11.z;
    ret.cos_omega_ne[3] = data11.w;
    const uvec4 data12 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 12));
    ret.cos_omega_ne[4] = data12.x;
    ret.cos_omega_ne[5] = data12.y;
    ret.cos_omega_ne[6] = data12.z;
    ret.cos_omega_ne[7] = data12.w;
    return ret;
}

light_cwbvh_node_t FetchLightCWBVHNode(samplerBuffer nodes_buf, const int li) {
    light_cwbvh_node_t ret;
    const vec3 data0 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 0).xyz;
    ret.bbox_min[0] = data0[0];
    ret.bbox_min[1] = data0[1];
    ret.bbox_min[2] = data0[2];
    const vec3 data1 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 1).xyz;
    ret.bbox_max[0] = data1[0];
    ret.bbox_max[1] = data1[1];
    ret.bbox_max[2] = data1[2];
    const uvec4 data2 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 2));
    ret.ch_bbox_min[0][0] = data2[0];
    ret.ch_bbox_min[0][1] = data2[1];
    ret.ch_bbox_min[1][0] = data2[2];
    ret.ch_bbox_min[1][1] = data2[3];
    const uvec4 data3 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 3));
    ret.ch_bbox_min[2][0] = data3[0];
    ret.ch_bbox_min[2][1] = data3[1];
    ret.ch_bbox_max[0][0] = data3[2];
    ret.ch_bbox_max[0][1] = data3[3];
    const uvec4 data4 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 4));
    ret.ch_bbox_max[1][0] = data4[0];
    ret.ch_bbox_max[1][1] = data4[1];
    ret.ch_bbox_max[2][0] = data4[2];
    ret.ch_bbox_max[2][1] = data4[3];
    const uvec4 data5 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 5));
    ret.child[0] = data5.x;
    ret.child[1] = data5.y;
    ret.child[2] = data5.z;
    ret.child[3] = data5.w;
    const uvec4 data6 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 6));
    ret.child[4] = data6.x;
    ret.child[5] = data6.y;
    ret.child[6] = data6.z;
    ret.child[7] = data6.w;
    const vec4 data7 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 7);
    ret.flux[0] = data7.x;
    ret.flux[1] = data7.y;
    ret.flux[2] = data7.z;
    ret.flux[3] = data7.w;
    const vec4 data8 = texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 8);
    ret.flux[4] = data8.x;
    ret.flux[5] = data8.y;
    ret.flux[6] = data8.z;
    ret.flux[7] = data8.w;
    const uvec4 data9 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 9));
    ret.axis[0] = data9.x;
    ret.axis[1] = data9.y;
    ret.axis[2] = data9.z;
    ret.axis[3] = data9.w;
    const uvec4 data10 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 10));
    ret.axis[4] = data10.x;
    ret.axis[5] = data10.y;
    ret.axis[6] = data10.z;
    ret.axis[7] = data10.w;
    const uvec4 data11 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 11));
    ret.cos_omega_ne[0] = data11.x;
    ret.cos_omega_ne[1] = data11.y;
    ret.cos_omega_ne[2] = data11.z;
    ret.cos_omega_ne[3] = data11.w;
    const uvec4 data12 = floatBitsToUint(texelFetch(nodes_buf, li * LIGHT_NODES_BUF_STRIDE + 12));
    ret.cos_omega_ne[4] = data12.x;
    ret.cos_omega_ne[5] = data12.y;
    ret.cos_omega_ne[6] = data12.z;
    ret.cos_omega_ne[7] = data12.w;
    return ret;
}

int PickLightSource(const vec3 P, samplerBuffer nodes_buf, const uint lights_count, float light_pick_rand, out float pdf_factor) {
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

        cur = n.child[next];
    }
    return int(cur & PRIM_INDEX_BITS);
#else // USE_HIERARCHICAL_NEE
    pdf_factor = 1.0 / float(lights_count);
    return clamp(int(light_pick_rand * lights_count), 0, int(lights_count - 1));
#endif // USE_HIERARCHICAL_NEE
}

shared float g_stack_factors[64][MAX_STACK_SIZE];

float EvalTriLightFactor(const vec3 P, samplerBuffer nodes_buf, samplerBuffer lights_buf, const uint lights_count, const uint tri_index, const vec3 ro) {
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
                    g_stack[gl_LocalInvocationIndex][stack_size++] = n.child[j];
                }
            }
        } else {
            const int light_index = int(cur & PRIM_INDEX_BITS);

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