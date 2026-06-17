#ifndef SUN_BRIGHTNESS_INTERFACE_H
#define SUN_BRIGHTNESS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SunBrightness)

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint TRANSMITTANCE_LUT_SLOT = 1;
const uint MULTISCATTER_LUT_SLOT = 2;
const uint MOON_TEX_SLOT = 3;
const uint WEATHER_TEX_SLOT = 4;
const uint CIRRUS_TEX_SLOT = 5;
const uint NOISE3D_TEX_SLOT = 6;

const uint OUT_BUF_SLOT = 0;

INTERFACE_END

#endif // SUN_BRIGHTNESS_INTERFACE_H