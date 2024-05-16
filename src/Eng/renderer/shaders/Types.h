#ifndef TYPES_H
#define TYPES_H

#include "_interface_common.h"

INTERFACE_START(Types)

struct AtmosphereParams {
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
    float cirrus_clouds_amount;
    //
    float cirrus_clouds_height;
    float ozone_height_center;
    float ozone_half_width;
    float atmosphere_density;
    //
    float stars_brightness;
    float moon_radius;
    float moon_distance;
    float _unused;
    //
    vec4 moon_dir;
    vec4 rayleigh_scattering;
    vec4 mie_scattering;
    vec4 mie_extinction;
    vec4 mie_absorption;
    vec4 ozone_absorbtion;
    vec4 ground_albedo;
};

INTERFACE_END

#endif // TYPES_H