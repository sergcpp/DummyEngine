#ifndef GBUFFER_SHADE_INTERFACE_H
#define GBUFFER_SHADE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GBufferShade)

struct Params {
    uvec2 img_size;
    float pixel_spread_angle;
    float _pad;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 1;
const uint DEPTH_LIN_TEX_SLOT = 2;
const uint ALBEDO_TEX_SLOT = 3;
const uint NORM_TEX_SLOT = 4;
const uint SPEC_TEX_SLOT = 5;

const uint SHADOW_DEPTH_TEX_SLOT = 6;
const uint SHADOW_DEPTH_VAL_TEX_SLOT = 7;
const uint SHADOW_COLOR_TEX_SLOT = 8;
const uint SSAO_TEX_SLOT = 9;
const uint LIGHT_BUF_SLOT = 10;
const uint DECAL_BUF_SLOT = 11;
const uint CELLS_BUF_SLOT = 12;
const uint ITEMS_BUF_SLOT = 13;
const uint GI_TEX_SLOT = 14;
const uint SUN_SHADOW_TEX_SLOT = 15;
const uint LTC_LUTS_TEX_SLOT = 16;
const uint ENV_TEX_SLOT = 17;

const uint OUT_COLOR_IMG_SLOT = 0;

INTERFACE_END

#endif // GBUFFER_SHADE_INTERFACE_H