R"(#version 310 es
#extension GL_EXT_texture_cube_map_array : enable

#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = )" AS_STR(REN_BASE0_TEX_SLOT) R"() uniform mediump samplerCubeArray env_texture;

layout(location = 1) uniform float mip_level;
layout(location = 2) uniform int probe_index;

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
    vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[)" AS_STR(REN_MAX_PROBES_TOTAL) R"(];
};

in vec3 aVertexPos_;

layout(location = )" AS_STR(REN_OUT_COLOR_INDEX) R"() out vec4 outColor;

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
}

void main() {
    vec3 view_dir_ws = normalize(aVertexPos_ - uProbes[probe_index].pos_and_radius.xyz);

    if (mip_level < 5.0) {
        // debug environment map
        outColor.rgb = RGBMDecode(textureLod(env_texture, vec4(view_dir_ws, uProbes[probe_index].unused_and_layer.w), mip_level));
    } else {
        const float SH_A0 = 0.886226952; // PI / sqrt(4.0f * Pi)
        const float SH_A1 = 1.02332675;  // sqrt(PI / 3.0f)

        // debug sh coeffs
        vec4 vv = vec4(SH_A0, SH_A1 * view_dir_ws.yzx);

        outColor.r = dot(uProbes[probe_index].sh_coeffs[0], vv);
        outColor.g = dot(uProbes[probe_index].sh_coeffs[1], vv);
        outColor.b = dot(uProbes[probe_index].sh_coeffs[2], vv);

        outColor.rgb = max(outColor.rgb, vec3(0.0));
    }

    outColor.a = 1.0;
}
)"