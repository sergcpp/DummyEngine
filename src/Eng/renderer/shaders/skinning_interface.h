#ifndef SKINNING_INTERFACE_H
#define SKINNING_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Skinning)

struct Params {
    uvec4 uSkinParams;
    uvec4 uShapeParamsCurr;
    uvec4 uShapeParamsPrev;
};

const int GRP_SIZE = 128;

const int IN_VERTICES_SLOT = 0;
const int IN_MATRICES_SLOT = 1;
const int IN_SHAPE_KEYS_SLOT = 2;
const int IN_DELTAS_SLOT = 3;
const int OUT_VERTICES0 = 4;
const int OUT_VERTICES1 = 5;

INTERFACE_END

#endif // SKINNING_INTERFACE_H