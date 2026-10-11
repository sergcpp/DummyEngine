#ifndef TYPES_H
#define TYPES_H

#include "_interface_common.h"

INTERFACE_START(Types)

struct atmosphere_params_t {
    float planet_radius;
    float viewpoint_height;
    float atmosphere_height;
    float rayleigh_height;
    //
    float mie_height;
    float clouds_height_beg;
    float clouds_height_end;
    float clouds_variety;
    //
    float clouds_density;
    float clouds_offset_x;
    float clouds_offset_z;
    float clouds_flutter_x;
    //
    float clouds_flutter_z;
    float cirrus_clouds_amount;
    float cirrus_clouds_height;
    float ozone_height_center;
    //
    float ozone_half_width;
    float atmosphere_density;
    float stars_brightness;
    float moon_radius;
    //
    float moon_distance;
    float _unused0;
    float _unused1;
    float _unused2;
    //
    vec4 moon_dir;
    vec4 rayleigh_scattering;
    vec4 mie_scattering;
    vec4 mie_extinction;
    vec4 mie_absorption;
    vec4 ozone_absorption;
    vec4 ground_albedo;
};

struct probe_volume_t {
    vec4 origin;
    vec4 spacing;
    ivec4 scroll;
    ivec4 scroll_diff;
};

struct light_item_t {
    float col[3];
    uint type_and_flags;
    float pos[3], radius;
    float dir[3], spot;
    float u[3];
    int shadowreg_index;
    float v[3], blend;
    float shadow_pos[3];
    uint tri_index;
};
const uint LIGHTS_BUF_STRIDE = 6;

struct cwbvh_node_t {
    float bbox_min[3];
    uint ch_bbox_min[3][2]; // per axis, 8 quantized bytes packed as 2 uints
    uint ch_bbox_max[3][2];
    uint ch_exp;           // 4 packed int8 quantization-step exponents
    uint child_base_imask; // low 24 bits child_base, high 8 bits internal-child slot mask
    uint prim_base;        // leaf payload base
    uint meta4_0;          // meta4 bytes for slots 0..3
    uint meta4_1;          // meta4 bytes for slots 4..7
};

struct light_cwbvh_node_t {
    float bbox_min[3];
    uint ch_bbox_min[3][2];
    uint ch_bbox_max[3][2];
    uint ch_exp;
    uint child_base_imask;
    uint prim_base;
    uint meta4_0;
    uint meta4_1;
    uint ch_bitmask[2];   // 8 bytes packed as 2 uints - child light-type filter, not validity
    uint flux[2];         // 8 bytes packed as 2 uints - fraction of the parent's flux, cube-coded
    uint axis[4];         // 8 packed (x, y) 8-bit snorm octahedral pairs
    uint cos_omega_ne[4]; // 8 packed (cos omega_n, cos omega_e) 8-bit snorm pairs
};

const int LIGHT_NODES_BUF_STRIDE = 8;

INTERFACE_END

#endif // TYPES_H