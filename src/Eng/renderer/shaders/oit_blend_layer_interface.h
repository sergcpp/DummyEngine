#ifndef OIT_BLEND_LAYER_INTERFACE_H
#define OIT_BLEND_LAYER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(OITBlendLayer)

const uint SHADOW_TEX_SLOT = 4;
const uint SPEC_TEX_SLOT = 5;
const uint LIGHT_BUF_SLOT = 6;
const uint DECAL_BUF_SLOT = 7;
const uint CELLS_BUF_SLOT = 8;
const uint ITEMS_BUF_SLOT = 9;
const uint LTC_LUTS_TEX_SLOT = 10;
const uint ENV_TEX_SLOT = 11;
const uint OIT_DEPTH_BUF_SLOT = 12;
const uint BACK_COLOR_TEX_SLOT = 16;
const uint BACK_DEPTH_TEX_SLOT = 17;

const uint IRRADIANCE_TEX_SLOT = 18;
const uint DISTANCE_TEX_SLOT = 19;
const uint OFFSET_TEX_SLOT = 20;

INTERFACE_END

#endif // OIT_BLEND_LAYER_INTERFACE_H