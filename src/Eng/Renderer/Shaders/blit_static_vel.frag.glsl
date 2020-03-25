R"(#version 310 es

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct ProbeItem {
    vec4 pos_and_radius;
    vec4 unused_and_layer;
    vec4 sh_coeffs[3];
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol, uTaaInfo;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
};

layout(binding = 0) uniform mediump sampler2D depth_texture;

in vec2 aVertexUVs_;

out vec2 outVelocity;

void main() {
    ivec2 pix_uvs = ivec2(aVertexUVs_);

    float depth = texelFetch(depth_texture, pix_uvs, 0).r;

    vec4 point_cs = vec4(aVertexUVs_.xy / uResAndFRes.xy, depth, 1.0);
    point_cs.xyz = 2.0 * point_cs.xyz - vec3(1.0);

    vec4 point_ws = uInvViewProjMatrix * point_cs;
    point_ws /= point_ws.w;

    vec4 point_prev_cs = uViewProjPrevMatrix * point_ws;
    point_prev_cs /= point_prev_cs.w;

    outVelocity = point_cs.xy + uTaaInfo.xy - point_prev_cs.xy;
}
)"
