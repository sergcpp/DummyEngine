#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    BatchDataBlock : $ubBatchDataLoc
*/

layout(location = $VtxPosLoc) in vec3 aVertexPosition;
layout(location = $VtxNorLoc) in vec4 aVertexNormal;
layout(location = $VtxTanLoc) in vec2 aVertexTangent;
layout(location = $VtxUV1Loc) in vec2 aVertexUVs1;
layout(location = $VtxAUXLoc) in uint aVertexColorPacked;

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

layout (location = $uInstancesLoc) uniform ivec4 uInstanceIndices[$MaxBatchSize / 4];

layout(binding = $InstanceBufSlot) uniform highp samplerBuffer instances_buffer;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out highp vec3 aVertexPos_;
layout(location = 1) out mediump vec2 aVertexUVs_;
layout(location = 2) out mediump vec3 aVertexNormal_;
layout(location = 3) out mediump vec3 aVertexTangent_;
layout(location = 4) out highp vec3 aVertexShUVs_[4];
#else
out highp vec3 aVertexPos_;
out mediump vec2 aVertexUVs_;
out mediump vec3 aVertexNormal_;
out mediump vec3 aVertexTangent_;
out highp vec3 aVertexShUVs_[4];
#endif

#ifdef VULKAN
    #define gl_InstanceID gl_InstanceIndex
#endif

vec4 SmoothCurve(vec4 x) {
    return x * x * (3.0 - 2.0 * x);
}

vec4 TriangleWave(vec4 x) {
    return abs(fract(x + 0.5) * 2.0 - 1.0);
}

vec4 SmoothTriangleWave(vec4 x) {
    return SmoothCurve(TriangleWave(x));
}

void main(void) {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    // load model matrix
    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

    vec3 vtx_pos_ws = (MMatrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((MMatrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((MMatrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

	// temp
    vec2 wind_vec = uWindParams.xz;
    float wind_phase = 0.0;//dot(MMatrix[3].xyz, vec3(1.0));
	
	vec3 vtx_color = unpackUnorm4x8(aVertexColorPacked).xyz;

    {   // Main bending
        const highp float bend_scale = 0.035;
        highp float bend_factor = vtx_pos_ws.y * bend_scale;
        // smooth bending factor and increase its nearby height limit
        bend_factor += 1.0;
        bend_factor *= bend_factor;
        bend_factor = bend_factor * bend_factor - bend_factor;
        // displace position
        highp vec3 new_pos = vtx_pos_ws.xyz;
        new_pos.xz += wind_vec * bend_factor;
        // rescale
        vtx_pos_ws.xyz = normalize(new_pos) * length(vtx_pos_ws.xyz);
    }

    {   // Branch/detail bending
        highp float branch_atten = vtx_color.r;
        highp float edge_atten = vtx_color.g;

        highp float branch_phase = wind_phase + vtx_color.b;
        highp float vtx_phase = dot(vtx_pos_ws.xyz, vec3(branch_phase));

        highp vec2 _waves = vec2(uTranspParamsAndTime.w) + vec2(vtx_phase, branch_phase);

        const highp float speed = 0.006;
        highp vec4 waves = (fract(_waves.xxyy * vec4(1.975, 0.793, 0.375, 0.193)) * vec4(2.0) - vec4(1.0)) * speed;
        waves = SmoothTriangleWave(waves);

        highp vec2 waves_sum = waves.xz + waves.yw;
        vtx_pos_ws.xyz += 64.0 * waves_sum.xyx * vec3(1.0 * edge_atten * vtx_nor_ws.x, branch_atten, 1.0 * edge_atten * vtx_nor_ws.z);
    }

    aVertexPos_ = vtx_pos_ws;
    aVertexNormal_ = vtx_nor_ws;
    aVertexTangent_ = vtx_tan_ws;
    aVertexUVs_ = aVertexUVs1;
    
    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );
    
    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        aVertexShUVs_[i] = (uShadowMapRegions[i].clip_from_world * vec4(vtx_pos_ws, 1.0)).xyz;
        aVertexShUVs_[i] = 0.5 * aVertexShUVs_[i] + 0.5;
        aVertexShUVs_[i].xy *= vec2(0.25, 0.5);
        aVertexShUVs_[i].xy += offsets[i];
    }
    
    gl_Position = uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
} 
