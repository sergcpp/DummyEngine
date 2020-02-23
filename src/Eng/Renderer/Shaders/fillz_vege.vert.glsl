R"(#version 310 es
#extension GL_EXT_texture_buffer : enable

)" __ADDITIONAL_DEFINES_STR__ R"(

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

layout(location = )" AS_STR(REN_VTX_POS_LOC) R"() in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = )" AS_STR(REN_VTX_UV1_LOC) R"() in vec2 aVertexUVs1;
#endif
layout(location = )" AS_STR(REN_VTX_NOR_LOC) R"() in vec3 aVertexNormal;
layout(location = )" AS_STR(REN_VTX_AUX_LOC) R"() in uint aVertexColorPacked;

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
    vec4 uWindParams;
};

layout(binding = )" AS_STR(REN_INST_BUF_SLOT) R"() uniform mediump samplerBuffer instances_buffer;
layout(location = )" AS_STR(REN_U_INSTANCES_LOC) R"() uniform ivec4 uInstanceIndices[)" AS_STR(REN_MAX_BATCH_SIZE) R"( / 4];

#ifdef TRANSPARENT_PERM
out vec2 aVertexUVs1_;
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

void main() {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
#endif

    vec3 vtx_pos_ws = (MMatrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((MMatrix * vec4(aVertexNormal.xyz, 0.0)).xyz);

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
        highp vec3 new_pos = vtx_pos_ws;
        new_pos.xz += wind_vec * bend_factor;
        // rescale
        vtx_pos_ws = normalize(new_pos) * length(vtx_pos_ws);
    }

    {   // Branch/detail bending
        highp float branch_atten = vtx_color.r;
        highp float edge_atten = vtx_color.g;

        highp float branch_phase = wind_phase + vtx_color.b;
        highp float vtx_phase = dot(vtx_pos_ws, vec3(branch_phase));

        highp vec2 _waves = vec2(uTranspParamsAndTime.w) + vec2(vtx_phase, branch_phase);

        const highp float speed = 0.006;
        highp vec4 waves = (fract(_waves.xxyy * vec4(1.975, 0.793, 0.375, 0.193)) * vec4(2.0) - vec4(1.0)) * speed;
        waves = SmoothTriangleWave(waves);

        highp vec2 waves_sum = waves.xz + waves.yw;
        vtx_pos_ws += 64.0 * waves_sum.xyx * vec3(1.0 * edge_atten * vtx_nor_ws.x, branch_atten, 1.0 * edge_atten * vtx_nor_ws.z);
    }

    gl_Position = uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
} 
)"