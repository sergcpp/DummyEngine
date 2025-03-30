#version 430 core

#include "_fs_common.glsl"
#include "vol_common.glsl"
#include "blit_vol_compose_interface.h"

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = FROXELS_TEX_SLOT) uniform sampler3D g_froxels_tex;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(location = 0) in vec2 g_vtx_uvs;
layout(location = 0) out vec4 g_out_color;

void main() {
    const float depth = textureLod(g_depth_tex, g_vtx_uvs, 0.0).x;
    const float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);

    const vec3 pos_ss = vec3(g_vtx_uvs, depth);
    const vec4 pos_cs = vec4(2.0 * pos_ss.xy - 1.0, pos_ss.z, 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec3 uvw = cs_to_uvw(pos_cs.xyz, lin_depth);
    const vec4 scattering_transmittance = SampleTricubic(g_froxels_tex, uvw, g_params.froxel_res.xyz);

    g_out_color = scattering_transmittance;
}
