#ifndef SKYDOME_INTERFACE_H
#define SKYDOME_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Skydome)

struct Params {
    mat4 clip_from_world;
    ivec2 sample_coord;
    ivec2 img_size;
};

struct Params2 {
    ivec2 sample_coord;
    ivec2 img_size;
    float hist_weight;
    float _pad[3];
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int ENV_TEX_SLOT = 1;
const int TRANSMITTANCE_LUT_SLOT = 2;
const int MULTISCATTER_LUT_SLOT = 3;
const int MOON_TEX_SLOT = 4;
const int WEATHER_TEX_SLOT = 5;
const int CIRRUS_TEX_SLOT = 6;
const int CURL_TEX_SLOT = 7;
const int NOISE3D_TEX_SLOT = 8;
const int DEPTH_TEX_SLOT = 9;
const int SKY_TEX_SLOT = 10;
const int SKY_HIST_TEX_SLOT = 11;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // SKYDOME_INTERFACE_H