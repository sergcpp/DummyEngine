#version 430 core
#extension GL_EXT_control_flow_attributes : require

#include "_cs_common.glsl"
#include "pmj_common.glsl"
#include "vol_common.glsl"

#include "vol_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;
layout(binding = FR_EMISSION_DENSITY_TEX_SLOT) uniform sampler3D g_fr_emission_density_history_tex;

layout(binding = OUT_FROXELS_IMG_SLOT, rgba16f) uniform writeonly image3D g_out_froxels_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec3 icoord = ivec3(gl_GlobalInvocationID.xyz);
    if (any(greaterThanEqual(icoord, g_params.froxel_res.xyz))) {
        return;
    }

    const uint px_hash = hash((gl_GlobalInvocationID.x << 16) | gl_GlobalInvocationID.y);
    const float offset_rand = get_scrambled_2d_rand(g_random_seq, RAND_DIM_VOL_OFFSET, px_hash, int(g_params.frame_index)).x;

    const vec3 pos_uvw = froxel_to_uvw(icoord, offset_rand, g_params.froxel_res.xyz);
    const vec4 pos_cs = vec4(2.0 * pos_uvw.xy - 1.0, pos_uvw.z, 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const float is_inside_volume = float(bbox_test(pos_ws, g_params.bbox_min.xyz, g_params.bbox_max.xyz));

    vec4 emission_density = vec4(g_params.emission_color.xyz, g_params.density) * is_inside_volume;
    emission_density.xyz = compress_hdr(emission_density.xyz, g_shrd_data.cam_pos_and_exp.w);

    { // history accumulation
        const vec3 pos_uvw_no_offset = froxel_to_uvw(icoord, 0.5, g_params.froxel_res.xyz);
        const vec4 pos_cs_no_offset = vec4(2.0 * pos_uvw_no_offset.xy - 1.0, pos_uvw_no_offset.z, 1.0);
        const vec3 pos_ws_no_offset = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs_no_offset);

        const vec3 hist_pos_cs = TransformToClipSpace(g_shrd_data.prev_clip_from_world, pos_ws_no_offset);
        const float hist_lin_depth = LinearizeDepth(hist_pos_cs.z, g_shrd_data.clip_info);
        const vec3 hist_uvw = cs_to_uvw(hist_pos_cs, hist_lin_depth);

        if (all(greaterThanEqual(hist_uvw, vec3(0.0))) && all(lessThanEqual(hist_uvw, vec3(1.0)))) {
            vec4 hist_fetch = textureLod(g_fr_emission_density_history_tex, hist_uvw, 0.0);
            hist_fetch.xyz *= g_params.hist_weight;

            const float HistoryWeight = 0.95;

            emission_density = mix(emission_density, hist_fetch, HistoryWeight);
        }
    }

    imageStore(g_out_froxels_img, icoord, emission_density);
}
