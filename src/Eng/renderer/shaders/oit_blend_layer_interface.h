#ifndef OIT_BLEND_LAYER_INTERFACE_H
#define OIT_BLEND_LAYER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(OITBlendLayer)

const int SHADOW_TEX_SLOT = 4;
const int SPEC_TEX_SLOT = 5;
const int LIGHT_BUF_SLOT = 6;
const int DECAL_BUF_SLOT = 7;
const int CELLS_BUF_SLOT = 8;
const int ITEMS_BUF_SLOT = 9;
const int LTC_LUTS_TEX_SLOT = 10;
const int ENV_TEX_SLOT = 11;
const int OIT_DEPTH_BUF_SLOT = 12;
const int BACK_COLOR_TEX_SLOT = 16;
const int BACK_DEPTH_TEX_SLOT = 17;

const int IRRADIANCE_TEX_SLOT = 18;
const int DISTANCE_TEX_SLOT = 19;
const int OFFSET_TEX_SLOT = 20;

INTERFACE_END

#endif // OIT_BLEND_LAYER_INTERFACE_H