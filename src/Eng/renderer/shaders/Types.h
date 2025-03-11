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
    vec4 ozone_absorbtion;
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
const int LIGHTS_BUF_STRIDE = 6;

struct light_wbvh_node_t {
    float bbox_min[3][8]; // SoA layout
    float bbox_max[3][8];
    uint child[8];
    float flux[8];
    uint axis[8];
    uint cos_omega_ne[8];
};

struct light_cwbvh_node_t {
    float bbox_min[3];
    float _unused0;
    float bbox_max[3];
    float _unused1;
    uint ch_bbox_min[3][2];
    uint ch_bbox_max[3][2];
    uint child[8];
    float flux[8];
    uint axis[8];
    uint cos_omega_ne[8];
};

const int LIGHT_NODES_BUF_STRIDE = 13;

INTERFACE_END

#endif // TYPES_H