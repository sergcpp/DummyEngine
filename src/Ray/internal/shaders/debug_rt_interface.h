#ifndef DEBUG_RT_INTERFACE_H
#define DEBUG_RT_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DebugRT)

struct Params {
    uvec2 img_size;
    uint node_index;
    float random_val;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int TRIS_BUF_SLOT = 1;
const int TRI_INDICES_BUF_SLOT = 2;
const int NODES_BUF_SLOT = 3;
const int MESH_INSTANCES_BUF_SLOT = 4;
const int RAYS_BUF_SLOT = 5;
const int TLAS_SLOT = 6;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // DEBUG_RT_INTERFACE_H