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
	vec4 uWindParams;
    ProbeItem uProbes[$MaxProbes];
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 4) in vec2 aVertexUVs1_;
#else
in vec2 aVertexUVs1_;
#endif

layout(location = $OutColorIndex) out vec4 outColor;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main(void) {
    vec4 tex_color = texture(diffuse_texture, aVertexUVs1_);
    float sig_dist = median(tex_color.r, tex_color.g, tex_color.b);
    
    const bool OUTLINE = false;
    const float OUTLINE_MIN_VALUE0 = 0.49;
    const float OUTLINE_MIN_VALUE1 = 0.49;
    const float OUTLINE_MAX_VALUE0 = 0.55;
    const float OUTLINE_MAX_VALUE1 = 0.58;
    const vec4 OUTLINE_COLOR = vec4(0.0, 0.0, 0.0, 1.0);
    
    vec4 base_color = vec4(1.0);
    
    if (OUTLINE && sig_dist >= OUTLINE_MIN_VALUE0 && sig_dist <= OUTLINE_MAX_VALUE1) {
        float factor = 1.0;
        if (sig_dist <= OUTLINE_MIN_VALUE1) {
            factor = smoothstep(OUTLINE_MIN_VALUE0, OUTLINE_MIN_VALUE1, sig_dist);
        } else {
            factor = smoothstep(OUTLINE_MAX_VALUE1, OUTLINE_MAX_VALUE0, sig_dist);
        }
        base_color = mix(base_color, OUTLINE_COLOR, factor);
    }
    
    const bool SOFT_EDGES = true;
    const float SOFT_EDGE_MIN = 0.35;
    const float SOFT_EDGE_MAX = 0.65;
    
    if (SOFT_EDGES) {
        float w = fwidth(sig_dist);
        base_color.a = smoothstep(0.5 - w, 0.5 + w, sig_dist);
        //base_color.a = smoothstep(SOFT_EDGE_MIN, SOFT_EDGE_MAX, sig_dist);
    } else {
        base_color.a = float(sig_dist >= 0.5);
    }
    
    // use distance (not pseudodistance) for glow
    sig_dist = tex_color.a;
    
    const bool OUTER_GLOW = false;
    const float OUTER_GLOW_MIN_DVALUE = 0.05;
    const float OUTER_GLOW_MAX_DVALUE = 0.4;
    const vec4 OUTER_GLOW_COLOR = vec4(1.0, 0.0, 0.0, 1.0);
    
    if (OUTER_GLOW) {
        vec4 glowc = OUTER_GLOW_COLOR * smoothstep(OUTER_GLOW_MIN_DVALUE, OUTER_GLOW_MAX_DVALUE, sig_dist);
        base_color = mix(glowc, vec4(base_color.rgb, 1.0), base_color.a);
    }
    
    outColor = base_color;
}
