#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
    precision highp float;
#endif

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    UniformParams : $ubUnifParamLoc
*/

#include "_fs_common.glsl"
#include "blit_static_vel_interface.glsl"

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D depth_texture;

LAYOUT(location = 0) in highp vec2 aVertexUVs_;

layout(location = 0) out highp vec2 outVelocity;

void main() {
    ivec2 pix_uvs = ivec2(aVertexUVs_);

    float depth = texelFetch(depth_texture, pix_uvs, 0).r;

    vec4 point_cs = vec4(aVertexUVs_.xy / shrd_data.uResAndFRes.xy, depth, 1.0);
#if defined(VULKAN)
	point_cs.xy = 2.0 * point_cs.xy - vec2(1.0);
#else
    point_cs.xyz = 2.0 * point_cs.xyz - vec3(1.0);
#endif

    vec4 point_ws = shrd_data.uInvViewProjMatrix * point_cs;
    point_ws /= point_ws.w;

    vec4 point_prev_cs = shrd_data.uViewProjPrevMatrix * point_ws;
    point_prev_cs /= point_prev_cs.w;

	vec2 unjitter = shrd_data.uTaaInfo.xy;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif
    outVelocity = 0.5 * (point_cs.xy + unjitter - point_prev_cs.xy);
}

