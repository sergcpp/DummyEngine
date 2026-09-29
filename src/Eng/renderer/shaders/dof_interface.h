#ifndef DOF_INTERFACE_H
#define DOF_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Dof)

struct Params {
    vec4 focal_params;   // x: focus distance (m), y: COC factor (halfres px), z: focal length (m),
                         // w: max kernel radius (halfres px)
    vec4 img_size;       // xy: size of processed image, zw: color texture size
    vec4 depth_size;     // xy: depth texture size
    vec4 clip_info;
    vec4 rot;            // sampling rotation mat2 packed as columns into xy/zw
    float rot_angle;     // base angle for upsample quad rotations
    uint seed;           // frame-varying random seed
};

const uint TILE_RES = 16;

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint COLOR_TEX_SLOT = 1;
const uint DEPTH_TEX_SLOT = 2;
const uint RANDOM_SEQ_BUF_SLOT = 3;
const uint COC_TILES_TEX_SLOT = 4;
const uint PRESORT_TEX_SLOT = 5;
const uint DOWNSAMPLED_TEX_SLOT = 6;
const uint FILTERED_TEX_SLOT = 7;
const uint ALPHA_TEX_SLOT = 8;

const uint OUT_IMG_SLOT = 0;
// NOTE: image unit indices must stay below GL_MAX_IMAGE_UNITS (8 on NVIDIA), unlike sampler slots which have a
// separate, much larger limit. Keep storage image bindings in the low range.
const uint OUT2_IMG_SLOT = 1;

INTERFACE_END

#endif // DOF_INTERFACE_H
