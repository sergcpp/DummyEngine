#ifndef SKYDOME_INTERFACE_H
#define SKYDOME_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Skydome)

struct Params {
    mat4 clip_from_world;
    uvec2 sample_coord;
    uvec2 img_size;
    vec2 texel_size;
    float scale;
    float _pad;
};

struct Params2 {
    uvec2 sample_coord;
    uvec2 img_size;
    vec2 texel_size;
    float hist_weight;
    float _pad;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint ENV_TEX_SLOT = 1;
const uint TRANSMITTANCE_LUT_SLOT = 2;
const uint MULTISCATTER_LUT_SLOT = 3;
const uint MOON_TEX_SLOT = 4;
const uint WEATHER_TEX_SLOT = 5;
const uint CIRRUS_TEX_SLOT = 6;
const uint CURL_TEX_SLOT = 7;
const uint NOISE3D_TEX_SLOT = 8;
const uint DEPTH_TEX_SLOT = 9;
const uint SKY_TEX_SLOT = 10;
const uint SKY_HIST_TEX_SLOT = 11;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // SKYDOME_INTERFACE_H