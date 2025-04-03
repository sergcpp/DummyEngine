#version 430 core

#include "_cs_common.glsl"
#include "_fs_common.glsl"
#include "vol_common.glsl"

#include "vol_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = FR_SCATTER_TEX_SLOT) uniform sampler3D g_fr_scatter_tex;
layout(binding = FR_EMISSION_TEX_SLOT) uniform sampler3D g_fr_emission_tex;

layout(binding = OUT_FR_FINAL_IMG_SLOT, rgba16f) uniform writeonly image3D g_out_fr_final_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    ivec3 icoord = ivec3(gl_GlobalInvocationID.xy, 0);
    if (any(greaterThanEqual(icoord.xy, g_params.froxel_res.xy))) {
        return;
    }

    vec4 radiance_transmittance = vec4(0.0, 0.0, 0.0, 1.0);

    float slice_dist_beg = vol_slice_distance(0, 0.0, g_params.froxel_res.z);
    for (icoord.z = 0; icoord.z < g_params.froxel_res.z; ++icoord.z) {
        const float slice_dist_end = vol_slice_distance(icoord.z + 1, 0.0, g_params.froxel_res.z);
        const float slice_depth = (slice_dist_end - slice_dist_beg);

        const vec4 emission_density = texelFetch(g_fr_emission_tex, icoord, 0);

        radiance_transmittance.xyz += slice_depth * radiance_transmittance.w * emission_density.xyz;
        if (emission_density.w > 0.0) {
            const vec4 scatter_absorption = texelFetch(g_fr_scatter_tex, icoord, 0);

            const float optical_depth = slice_depth * emission_density.w;
            const float local_transmittance = exp(-optical_depth * (1.0 - scatter_absorption.w));

            radiance_transmittance.xyz += radiance_transmittance.w * scatter_absorption.xyz * (1.0 - local_transmittance) / emission_density.w;
            radiance_transmittance.w *= local_transmittance;
        }

        slice_dist_beg = slice_dist_end;

        imageStore(g_out_fr_final_img, icoord, radiance_transmittance);
    }
}