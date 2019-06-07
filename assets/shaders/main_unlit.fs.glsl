#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#define LIGHT_ATTEN_CUTOFF 0.001f

layout(binding = $DiffTexSlot) uniform sampler2D diffuse_texture;

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
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes;
    ProbeItem uProbes[$MaxProbes];
};

#ifdef VULKAN
layout(location = 4) in vec2 aVertexUVs1_;
#else
in vec2 aVertexUVs1_;
#endif

layout(location = $OutColorIndex) out vec4 outColor;
layout(location = $OutSpecIndex) out vec4 outSpecular;

void main(void) {
    vec3 albedo_color = pow(texture(diffuse_texture, aVertexUVs1_).rgb, vec3(2.2));
    
    outColor = vec4(albedo_color, 1.0);
    outSpecular = vec4(0.0);
}
