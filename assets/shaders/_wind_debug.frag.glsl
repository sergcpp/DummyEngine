#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#define LIGHT_ATTEN_CUTOFF 0.004

layout(binding = $MatTex0Slot) uniform sampler2D diffuse_texture;
layout(binding = $MatTex1Slot) uniform sampler2D normals_texture;
layout(binding = $MatTex2Slot) uniform sampler2D specular_texture;
layout(binding = $ShadTexSlot) uniform sampler2DShadow shadow_texture;
layout(binding = $DecalTexSlot) uniform sampler2D decals_texture;
layout(binding = $SSAOTexSlot) uniform sampler2D ao_texture;
layout(binding = $LightBufSlot) uniform mediump samplerBuffer lights_buffer;
layout(binding = $DecalBufSlot) uniform mediump samplerBuffer decals_buffer;
layout(binding = $CellsBufSlot) uniform highp usamplerBuffer cells_buffer;
layout(binding = $ItemsBufSlot) uniform highp usamplerBuffer items_buffer;
layout(binding = $NoiseTexSlot) uniform sampler2D noise_texture;

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct ProbeItem {
    vec4 pos_and_radius;
    vec4 unused_and_layer;
    vec4 sh_coeffs[3];
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
	vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[$MaxProbes];
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_;
layout(location = 1) in mediump vec2 aVertexUVs_;
layout(location = 2) in mediump vec3 aVertexNormal_;
layout(location = 3) in mediump vec3 aVertexTangent_;
layout(location = 4) in highp vec3 aVertexShUVs_[4];
#else
in highp vec3 aVertexPos_;
in mediump vec2 aVertexUVs_;
in mediump vec3 aVertexNormal_;
in mediump vec3 aVertexTangent_;
in highp vec3 aVertexShUVs_[4];
#endif

layout(location = $OutColorIndex) out vec4 outColor;
layout(location = $OutNormIndex) out vec4 outNormal;
layout(location = $OutSpecIndex) out vec4 outSpecular;

#include "common.glsl"

void main(void) {
	vec3 lo_freq_bend_dir = 0.5 * texture(noise_texture, uWindScroll.xy + (1.0 / 256.0) * vec2(aVertexPos_.xz)).rgb + vec3(0.5);
	vec3 hi_freq_bend_dir = 0.5 * texture(noise_texture, uWindScroll.zw + (1.0 / 8.0) * vec2(aVertexPos_.xz)).rgb + vec3(0.5);

    outColor = vec4(pow(lo_freq_bend_dir + hi_freq_bend_dir, vec3(2.2)), 1.0);
    outNormal = vec4(0.0);
    outSpecular = vec4(0.0);
}
