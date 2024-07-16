#ifndef SUN_BRIGHTNESS_INTERFACE_H
#define SUN_BRIGHTNESS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SunBrightness)

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int TRANSMITTANCE_LUT_SLOT = 1;
const int MULTISCATTER_LUT_SLOT = 2;
const int MOON_TEX_SLOT = 3;
const int WEATHER_TEX_SLOT = 4;
const int CIRRUS_TEX_SLOT = 5;
const int NOISE3D_TEX_SLOT = 6;

const int OUT_BUF_SLOT = 0;

INTERFACE_END

#endif // SUN_BRIGHTNESS_INTERFACE_H