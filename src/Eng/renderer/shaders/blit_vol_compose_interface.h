#ifndef BLIT_VOL_COMPOSE_INTERFACE_H
#define BLIT_VOL_COMPOSE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(VolCompose)

struct Params {
    vec4 transform;
    vec4 froxel_res;
};

const int DEPTH_TEX_SLOT = 0;
const int FROXELS_TEX_SLOT = 1;

INTERFACE_END

#endif // BLIT_VOL_COMPOSE_INTERFACE_H