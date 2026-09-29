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
    vec2 vel_texel;      // xy: 1/ren_res - velocity textures store ren_res texel deltas (fillz), multiply to get
                         // UV; doubles as the std140 alignment pad (Ren vectors are 4-byte aligned) for the vec4 below

    vec4 lens_params;    // x: blade count (0 - round), y: anamorphic ratio (1 - no stretch), z: lens rotation
};

#define FOCAL_DEPTH g_params.focal_params.x
#define COC_FACTOR g_params.focal_params.y
#define FOCAL_LENGTH g_params.focal_params.z
#define MAX_KERNEL_RADIUS g_params.focal_params.w

const uint TILE_RES = 16;

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint COLOR_TEX_SLOT = 2;
const uint DEPTH_TEX_SLOT = 3;
const uint RANDOM_SEQ_BUF_SLOT = 4;
const uint COC_TILES_TEX_SLOT = 5;
const uint PRESORT_TEX_SLOT = 6;
const uint DOWNSAMPLED_TEX_SLOT = 7;
const uint FILTERED_TEX_SLOT = 8;
const uint ALPHA_TEX_SLOT = 9;
const uint VEL_TEX_SLOT = 10;
const uint PRESORT_HIST_TEX_SLOT = 11;

const uint OUT_IMG_SLOT = 0;
const uint OUT2_IMG_SLOT = 1;

INTERFACE_END

#endif // DOF_INTERFACE_H
