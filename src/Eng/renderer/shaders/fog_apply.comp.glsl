#version 430 core

#include "_cs_common.glsl"
#include "fog_common.glsl"

#include "fog_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = FROXELS_TEX_SLOT) uniform sampler3D g_froxels_tex;

layout(binding = INOUT_COLOR_IMG_SLOT, rgba16f) uniform image2D g_inout_color_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(icoord, g_params.img_res))) {
        return;
    }

    const float depth = texelFetch(g_depth_tex, icoord, 0).x;
    const float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);

    const vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;
    const vec3 pos_ss = vec3(norm_uvs, depth);
    const vec4 pos_cs = vec4(2.0 * pos_ss.xy - 1.0, pos_ss.z, 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec3 uvw = cs_to_uvw(pos_cs.xyz, lin_depth);
    const vec4 scattering_transmittance = SampleTricubic(g_froxels_tex, uvw, vec3(g_params.froxel_res.xyz));

    const vec4 background = imageLoad(g_inout_color_img, icoord);
    imageStore(g_inout_color_img, icoord, vec4(scattering_transmittance.w * background.xyz + scattering_transmittance.xyz, background.w));
}
