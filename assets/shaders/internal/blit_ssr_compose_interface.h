#ifndef BLIT_SSR_COMPOSE_INTERFACE_H
#define BLIT_SSR_COMPOSE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRCompose)

struct Params {
    VEC4_TYPE transform;
};

DEF_CONST_INT(SSR_TEX_SLOT, 0);

INTERFACE_END

#endif // BLIT_SSR_COMPOSE_INTERFACE_H