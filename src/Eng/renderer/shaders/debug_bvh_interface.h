#ifndef DEBUG_BVH_INTERFACE_H
#define DEBUG_BVH_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DebugBVH)

struct Params {
    uvec2 img_size;
    uint root_node;
    uint node_count;
    uint tree_depth;  // longest root-to-leaf path, drives the visit-count normalization
};

const uint GRP_SIZE_X = 16;
const uint GRP_SIZE_Y = 16;

// A clean traversal of a balanced tree visits a small multiple of its depth per ray, so the
// heat map saturates at VISIT_DEPTH_FACTOR levels of traversal cost
const uint VISIT_DEPTH_FACTOR = 4;

const uint NODES_BUF_SLOT = 1;
const uint DEPTH_TEX_SLOT = 2;

const uint OUT_IMG_SLOT = 0;

// phy::dyn_bvh_node_t (36 B) uploaded as u32 texels: bbox_min, leaf_or_left, bbox_max,
// axis_or_right, parent
const uint BVH_NODE_STRIDE = 9;
const uint BVH_NODE_LEAF_LEFT = 3;
const uint BVH_NODE_AXIS_RIGHT = 7;

const uint BVH_NO_NODE = 0xFFFFFFFFu;

INTERFACE_END

#endif // DEBUG_BVH_INTERFACE_H
