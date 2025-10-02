#ifndef RECONSTRUCT_NORMALS_INTERFACE_H
#define RECONSTRUCT_NORMALS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ReconstructNormals)

struct Params {
    vec4 frustum_info;
    vec4 clip_info;
    uvec2 img_size;
};

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;

const int OUT_NORMALS_IMG_SLOT = 0;

INTERFACE_END

#endif // RECONSTRUCT_NORMALS_INTERFACE_H