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
    uint frame_index;
};

struct Params2 {
    uvec2 sample_coord;
    uvec2 img_size;
    vec2 texel_size;
    float hist_weight;
    float _pad;
};

const uint CLOUD_SHADOWMAP_RES = 1024;

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint ENV_TEX_SLOT = 2;
const uint TRANSMITTANCE_LUT_SLOT = 3;
const uint MULTISCATTER_LUT_SLOT = 4;
const uint MOON_TEX_SLOT = 5;
const uint WEATHER_TEX_SLOT = 6;
const uint CIRRUS_TEX_SLOT = 7;
const uint CURL_TEX_SLOT = 8;
const uint NOISE3D_TEX_SLOT = 9;
const uint DEPTH_TEX_SLOT = 10;
const uint SKY_TEX_SLOT = 11;
const uint SKY_HIST_TEX_SLOT = 12;
const uint TCBN_1D_TEX_SLOT = 13;

const uint MOMENTS_B0_TEX_SLOT = 14;
const uint MOMENTS_B1234_TEX_SLOT = 15;

const uint MOMENTS_B0_HIST_TEX_SLOT = 2;
const uint MOMENTS_B1234_HIST_TEX_SLOT = 3;

const uint OUT_IMG_SLOT = 0;
const uint OUT_BUF_SLOT = 0;
const uint OUT_B0_IMG_SLOT = 0;
const uint OUT_B1234_IMG_SLOT = 1;

INTERFACE_END

#endif // SKYDOME_INTERFACE_H