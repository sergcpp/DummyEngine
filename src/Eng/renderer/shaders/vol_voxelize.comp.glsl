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

layout(binding = OUT_FR_EMISSION_DENSITY_IMG_SLOT, rgba16f) uniform writeonly image3D g_out_fr_emission_density_img;
layout(binding = OUT_FR_SCATTER_ABSORPTION_IMG_SLOT, rgba16f) uniform writeonly image3D g_out_fr_scatter_absorption_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    ivec3 icoord = ivec3(gl_GlobalInvocationID.xy, 0);
    if (any(greaterThanEqual(icoord.xy, g_params.froxel_res.xy))) {
        return;
    }

    const vec2 norm_uvs = (vec2(icoord.xy) + 0.5) / g_params.froxel_res.xy;
    const vec2 d = norm_uvs * 2.0 - 1.0;

    const vec3 origin_ws = g_shrd_data.cam_pos_and_exp.xyz;
    const vec3 target_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, vec4(d.xy, 1, 1));
    const vec3 direction_ws = normalize(target_ws - origin_ws);
    const vec3 inv_d = (1.0 / direction_ws);
    const vec3 neg_inv_d_o = -inv_d * origin_ws;

    const uint px_hash = hash((gl_GlobalInvocationID.x << 16) | gl_GlobalInvocationID.y);
    const float offset_rand = get_scrambled_2d_rand(g_random_seq, RAND_DIM_VOL_OFFSET, px_hash, int(g_params.frame_index)).x;

    vec2 bbox_intersection = bbox_test(inv_d, neg_inv_d_o, g_params.bbox_min.xyz, g_params.bbox_max.xyz);
    if (bbox_intersection.x < bbox_intersection.y && bbox_intersection.y > 0.0) {
        bbox_intersection.x = max(bbox_intersection.x, 0.0);

        bbox_intersection *= dot(direction_ws, g_shrd_data.frustum_planes[4].xyz);

        const vec2 z_bounds = log2(bbox_intersection / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];

        int z_beg = int(z_bounds.x * g_params.froxel_res.z + 0 * offset_rand);
        int z_end = min(int(z_bounds.y * g_params.froxel_res.z + 0 * offset_rand), g_params.froxel_res.z - 1);

        // TODO: Simplify this!
        float dist_beg = vol_slice_distance(z_beg, offset_rand, g_params.froxel_res.z);
        float dist_end = vol_slice_distance(z_end, offset_rand, g_params.froxel_res.z);

        if (bbox_intersection.x > dist_beg) ++z_beg;
        if (bbox_intersection.y < dist_end) --z_end;

        vec4 emission_density = vec4(g_params.emission_color.xyz, g_params.density);
        emission_density.xyz = compress_hdr(emission_density.xyz, g_shrd_data.cam_pos_and_exp.w);

        for (; icoord.z < z_beg; ++icoord.z) {
            imageStore(g_out_fr_emission_density_img, icoord, vec4(0.0));
            imageStore(g_out_fr_scatter_absorption_img, icoord, vec4(0.0));
        }
        for (; icoord.z <= z_end; ++icoord.z) {
            imageStore(g_out_fr_emission_density_img, icoord, emission_density);
            imageStore(g_out_fr_scatter_absorption_img, icoord, g_params.scatter_color);
        }
    }
    for (; icoord.z < g_params.froxel_res.z; ++icoord.z) {
        imageStore(g_out_fr_emission_density_img, icoord, vec4(0.0));
        imageStore(g_out_fr_scatter_absorption_img, icoord, vec4(0.0));
    }
}
