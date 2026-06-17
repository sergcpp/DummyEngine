#ifndef BLIT_SSR_COMPOSE_INTERFACE_H
#define BLIT_SSR_COMPOSE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRCompose)

struct Params {
    vec4 transform;
};

const uint ALBEDO_TEX_SLOT = 0;
const uint SPEC_TEX_SLOT = 1;
const uint DEPTH_TEX_SLOT = 2;
const uint NORM_TEX_SLOT = 3;
const uint REFL_TEX_SLOT = 4;
const uint LTC_LUTS_TEX_SLOT = 5;

INTERFACE_END

#endif // BLIT_SSR_COMPOSE_INTERFACE_H