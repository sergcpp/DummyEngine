R"(#version )" GLSL_VERSION_STR R"(

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
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
    vec4 uWindParams;
    ProbeItem uProbes[)" AS_STR(REN_MAX_PROBES_TOTAL) R"(];
};

struct VertexData0 {
    highp vec4 p_and_t0;
};

struct VertexData1 {
    highp uvec2 n_and_bx;
    highp uvec2 byz_and_c_packed;
};

struct VegeRegion {
    highp uint in_vtx_offset, out_vtx_offset;
    highp uint wind_vec_packed, wind_phase_and_vertex_count;
};

layout(std430, binding = 0) buffer InOut0 {
    VertexData0 vertices[];
} inout_data0;

layout(std430, binding = 1) buffer InOut1 {
    VertexData1 vertices[];
} inout_data1;

layout(std430, binding = 2) readonly buffer Input3 {
    VegeRegion vege_regions[];
} in_data2;

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

highp vec4 SmoothCurve(highp vec4 x) {
    return x * x * (3.0 - 2.0 * x);
}

highp vec4 TriangleWave(highp vec4 x) {
    return abs(fract(x + 0.5) * 2.0 - 1.0);
}

highp vec4 SmoothTriangleWave(highp vec4 x) {
    return SmoothCurve(TriangleWave(x));
}

#if 0
// Phases (object, vertex, branch)
float fObjPhase = dot(worldPos.xyz, 1);
fBranchPhase += fObjPhase;
float fVtxPhase = dot(vPos.xyz, fDetailPhase + fBranchPhase);
// x is used for edges; y is used for branches
float2 vWavesIn = fTime + float2(fVtxPhase, fBranchPhase );
// 1.975, 0.793, 0.375, 0.193 are good frequencies
float4 vWaves = (frac( vWavesIn.xxyy * float4(1.975, 0.793, 0.375, 0.193) ) * 2.0 - 1.0 ) * fSpeed * fDetailFreq;
vWaves = SmoothTriangleWave( vWaves );
float2 vWavesSum = vWaves.xz + vWaves.yw;
// Edge (xy) and branch bending (z)
vPos.xyz += vWavesSum.xxy * float3(fEdgeAtten * fDetailAmp * vNormal.xy, fBranchAtten * fBranchAmp); 
#endif

#if 0
// Bend factor - Wind variation is done on the CPU.
float fBF = vPos.z * fBendScale;
// Smooth bending factor and increase its nearby height limit.
fBF += 1.0;
fBF *= fBF;
fBF = fBF * fBF - fBF;
// Displace position
float3 vNewPos = vPos;
vNewPos.xy += vWind.xy * fBF;
// Rescale
vPos.xyz = normalize(vNewPos.xyz)* fLength; 
#endif

void main() {
    int reg_id = int(gl_WorkGroupID.x);
    mediump uint vertex_count = bitfieldExtract(in_data2.vege_regions[reg_id].wind_phase_and_vertex_count, 16, 16);
    if (gl_LocalInvocationID.x >= vertex_count) {
        return;
    }

    highp float wind_phase = unpackHalf2x16(in_data2.vege_regions[reg_id].wind_phase_and_vertex_count).x;
    mediump vec2 wind_vec = unpackSnorm2x16(in_data2.vege_regions[reg_id].wind_vec_packed);

    highp uint in_ndx = in_data2.vege_regions[reg_id].in_vtx_offset + gl_LocalInvocationID.x;
    highp vec3 p = inout_data0.vertices[in_ndx].p_and_t0.xyz;

    highp vec2 nxy = unpackSnorm2x16(inout_data1.vertices[in_ndx].n_and_bx.x);
    highp vec2 nz_and_bx = unpackSnorm2x16(inout_data1.vertices[in_ndx].n_and_bx.y);
    highp vec2 byz = unpackSnorm2x16(inout_data1.vertices[in_ndx].byz_and_c_packed.x);

    highp vec4 c = unpackUnorm4x8(inout_data1.vertices[in_ndx].byz_and_c_packed.y);

    highp vec3 n = vec3(nxy, nz_and_bx.x),
               b = vec3(nz_and_bx.y, byz);

    highp vec3 tr_p = p;

    {   // Main bending
        const highp float bend_scale = 0.035;
        highp float bend_factor = tr_p.y * bend_scale;
        // smooth bending factor and increase its nearby height limit
        bend_factor += 1.0;
        bend_factor *= bend_factor;
        bend_factor = bend_factor * bend_factor - bend_factor;
        // displace position
        highp vec3 new_pos = tr_p;
        new_pos.xz += wind_vec * bend_factor;
        // rescale
        tr_p = normalize(new_pos) * length(tr_p);
    }

    {   // Branch/detail bending
        highp float branch_atten = c.r;
        highp float edge_atten = c.g;

        highp float branch_phase = wind_phase + c.b;
        highp float vtx_phase = dot(tr_p, vec3(branch_phase));

        highp vec2 _waves = vec2(uTranspParamsAndTime.w) + vec2(vtx_phase, branch_phase);

        const highp float speed = 0.006;
        highp vec4 waves = (fract(_waves.xxyy * vec4(1.975, 0.793, 0.375, 0.193)) * vec4(2.0) - vec4(1.0)) * speed;
        waves = SmoothTriangleWave(waves);

        highp vec2 waves_sum = waves.xz + waves.yw;
        tr_p += 64.0 * waves_sum.xyx * vec3(edge_atten * n.x, branch_atten, edge_atten * n.z);
    }

    highp uint out_ndx = in_data2.vege_regions[reg_id].out_vtx_offset + gl_LocalInvocationID.x;

    inout_data0.vertices[out_ndx].p_and_t0.xyz = tr_p;
    // copy texture coordinates unchanged
    inout_data0.vertices[out_ndx].p_and_t0.w = inout_data0.vertices[in_ndx].p_and_t0.w;

    // copy rest of the attributes unchanged
    inout_data1.vertices[out_ndx].n_and_bx = inout_data1.vertices[in_ndx].n_and_bx;
    inout_data1.vertices[out_ndx].byz_and_c_packed = inout_data1.vertices[in_ndx].byz_and_c_packed;
}

)"
