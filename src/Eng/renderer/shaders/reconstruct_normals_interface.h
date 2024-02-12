#ifndef RECONSTRUCT_NORMALS_INTERFACE_H
#define RECONSTRUCT_NORMALS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ReconstructNormals)

struct Params {
    VEC4_TYPE frustum_info;
    VEC4_TYPE clip_info;
    UVEC2_TYPE img_size;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;

const int OUT_NORMALS_IMG_SLOT = 0;

INTERFACE_END

#endif // RECONSTRUCT_NORMALS_INTERFACE_H