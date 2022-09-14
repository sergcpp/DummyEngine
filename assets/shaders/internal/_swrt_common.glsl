
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
    uint vert_index, vert_count;
};

struct mesh_instance_t {
    vec4 bbox_min; // w is tr_index
    vec4 bbox_max; // w is mesh_index
    mat4 inv_transform;
};

struct hit_data_t {
    int mask;
    int obj_index;
    int prim_index;
    float t, u, v;
};

#define near_child(rd, n)   \
    (rd)[floatBitsToUint(n.bbox_max.w) >> 30] < 0 ? (floatBitsToUint(n.bbox_max.w) & RIGHT_CHILD_BITS) : floatBitsToUint(n.bbox_min.w)

#define far_child(rd, n)    \
    (rd)[floatBitsToUint(n.bbox_max.w) >> 30] < 0 ? floatBitsToUint(n.bbox_min.w) : (floatBitsToUint(n.bbox_max.w) & RIGHT_CHILD_BITS)


#undef EPS