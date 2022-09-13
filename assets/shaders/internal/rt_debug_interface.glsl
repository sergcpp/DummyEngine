#ifndef RT_DEBUG_INTERFACE_GLSL
#define RT_DEBUG_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(RTDebug)

struct Params {
    float pixel_spread_angle;
    float _pad[3];
};

struct RayPayload {
    VEC3_TYPE col;
    float cone_width;
};

const UINT_TYPE LEAF_NODE_BIT = (1u << 31);
const UINT_TYPE PRIM_INDEX_BITS = ~LEAF_NODE_BIT;
const UINT_TYPE LEFT_CHILD_BITS = ~LEAF_NODE_BIT;

const UINT_TYPE SEP_AXIS_BITS = (3u << 30); // 0b11u
const UINT_TYPE PRIM_COUNT_BITS = ~SEP_AXIS_BITS;
const UINT_TYPE RIGHT_CHILD_BITS = ~SEP_AXIS_BITS;

struct bvh_node_t {
    VEC4_TYPE bbox_min; // w is prim_index/left_child
    VEC4_TYPE bbox_max; // w is prim_count/right_child
};

struct hit_data_t {
    int mask;
    int obj_index;
    int prim_index;
    float t, u, v;
};

DEF_CONST_INT(MAX_STACK_SIZE, 48);

DEF_CONST_INT(LOCAL_GROUP_SIZE_X, 8)
DEF_CONST_INT(LOCAL_GROUP_SIZE_Y, 8)

DEF_CONST_INT(TLAS_SLOT, 1)
DEF_CONST_INT(ENV_TEX_SLOT, 2)
DEF_CONST_INT(GEO_DATA_BUF_SLOT, 3)
DEF_CONST_INT(MATERIAL_BUF_SLOT, 4)
DEF_CONST_INT(VTX_BUF1_SLOT, 5)
DEF_CONST_INT(VTX_BUF2_SLOT, 6)
DEF_CONST_INT(NDX_BUF_SLOT, 7)
DEF_CONST_INT(NODES_BUF_SLOT, 8)
DEF_CONST_INT(LMAP_TEX_SLOTS, 9)
DEF_CONST_INT(OUT_IMG_SLOT, 0)

INTERFACE_END

#endif // RT_DEBUG_INTERFACE_GLSL