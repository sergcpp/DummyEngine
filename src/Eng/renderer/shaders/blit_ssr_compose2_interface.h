#ifndef BLIT_SSR_COMPOSE2_INTERFACE_H
#define BLIT_SSR_COMPOSE2_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRCompose2)

struct Params {
    VEC4_TYPE transform;
};

const int SPEC_TEX_SLOT = 0;
const int DEPTH_TEX_SLOT = 1;
const int NORM_TEX_SLOT = 2;
const int REFL_TEX_SLOT = 3;
const int BRDF_TEX_SLOT = 4;

INTERFACE_END

#endif // BLIT_SSR_COMPOSE2_INTERFACE_H