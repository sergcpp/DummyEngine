#ifndef SKYDOME_INTERFACE_H
#define SKYDOME_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Skydome)

struct Params {
    mat4 xform;
    mat4 clip_from_world;
};

const int ENV_TEX_SLOT = 0;
const int TRANSMITTANCE_LUT_SLOT = 1;
const int MULTISCATTER_LUT_SLOT = 2;
const int MOON_TEX_SLOT = 3;
const int WEATHER_TEX_SLOT = 4;
const int CIRRUS_TEX_SLOT = 5;
const int NOISE3D_TEX_SLOT = 6;

INTERFACE_END

#endif // SKYDOME_INTERFACE_H