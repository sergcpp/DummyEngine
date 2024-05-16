#ifndef BLIT_SSR_COMPOSE_NEW_INTERFACE_H
#define BLIT_SSR_COMPOSE_NEW_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRComposeNew)

struct Params {
    vec4 transform;
};

const int ALBEDO_TEX_SLOT = 0;
const int SPEC_TEX_SLOT = 1;
const int DEPTH_TEX_SLOT = 2;
const int NORM_TEX_SLOT = 3;
const int REFL_TEX_SLOT = 4;
const int LTC_LUTS_TEX_SLOT = 5;

INTERFACE_END

#endif // BLIT_SSR_COMPOSE_NEW_INTERFACE_H