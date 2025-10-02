#ifndef GBUFFER_SHADE_INTERFACE_H
#define GBUFFER_SHADE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GBufferShade)

struct Params {
    uvec2 img_size;
    float pixel_spread_angle;
    float _pad;
};

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int DEPTH_LIN_TEX_SLOT = 2;
const int ALBEDO_TEX_SLOT = 3;
const int NORM_TEX_SLOT = 4;
const int SPEC_TEX_SLOT = 5;

const int SHADOW_DEPTH_TEX_SLOT = 6;
const int SHADOW_DEPTH_VAL_TEX_SLOT = 7;
const int SHADOW_COLOR_TEX_SLOT = 8;
const int SSAO_TEX_SLOT = 9;
const int LIGHT_BUF_SLOT = 10;
const int DECAL_BUF_SLOT = 11;
const int CELLS_BUF_SLOT = 12;
const int ITEMS_BUF_SLOT = 13;
const int GI_TEX_SLOT = 14;
const int SUN_SHADOW_TEX_SLOT = 15;
const int LTC_LUTS_TEX_SLOT = 16;
const int ENV_TEX_SLOT = 17;

const int OUT_COLOR_IMG_SLOT = 0;

INTERFACE_END

#endif // GBUFFER_SHADE_INTERFACE_H