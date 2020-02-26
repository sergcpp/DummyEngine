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
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
	vec4 uWindScroll;
    ProbeItem uProbes[$MaxProbes];
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 4) in vec2 aVertexUVs1_;
layout(location = 5) flat in float aUseSDF_;
#else
in vec2 aVertexUVs1_;
flat in float aUseSDF_;
#endif

layout(location = $OutColorIndex) out vec4 outColor;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main(void) {
    vec4 tex_color = texture(diffuse_texture, aVertexUVs1_);
    if (aUseSDF_ < 0.5) {
        outColor = tex_color;
    } else {
        float sig_dist = median(tex_color.r, tex_color.g, tex_color.b);
        
        outColor.rgb = vec3(1.0);
        
        float w = fwidth(sig_dist);
        outColor.a = smoothstep(0.5 - w, 0.5 + w, sig_dist);
        
        /*float s = sig_dist - 0.5;
        float v = s / fwidth(s);
        outColor.a = clamp(v + 0.5, 0.0, 1.0);*/
    }
}
