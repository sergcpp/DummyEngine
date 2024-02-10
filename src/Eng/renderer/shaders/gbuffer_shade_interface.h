#ifndef GBUFFER_SHADE_INTERFACE_H
#define GBUFFER_SHADE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GBufferShade)

struct Params {
    UVEC2_TYPE img_size;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int ALBEDO_TEX_SLOT = 2;
const int NORMAL_TEX_SLOT = 3;
const int SPECULAR_TEX_SLOT = 4;

const int SHADOW_TEX_SLOT = 5;
const int SSAO_TEX_SLOT = 6;
const int LIGHT_BUF_SLOT = 7;
const int DECAL_BUF_SLOT = 8;
const int CELLS_BUF_SLOT = 9;
const int ITEMS_BUF_SLOT = 10;
const int GI_TEX_SLOT = 11;
const int SUN_SHADOW_TEX_SLOT = 12;
const int LTC_LUTS_TEX_SLOT = 13;

const int OUT_COLOR_IMG_SLOT = 0;

INTERFACE_END

#endif // GBUFFER_SHADE_INTERFACE_H