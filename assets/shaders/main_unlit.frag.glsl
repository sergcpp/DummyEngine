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
    vec4 uSunDir, uSunCol, uTaaInfo;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
	vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[$MaxProbes];
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 4) in vec2 aVertexUVs1_;
#else
in vec2 aVertexUVs1_;
#endif

layout(location = $OutColorIndex) out vec4 outColor;
layout(location = $OutNormIndex) out vec4 outNormal;

void main(void) {
    vec3 albedo_color = texture(diffuse_texture, aVertexUVs1_).rgb;
    
    outColor = vec4(albedo_color, 1.0);
    outNormal = vec4(0.0);
}
