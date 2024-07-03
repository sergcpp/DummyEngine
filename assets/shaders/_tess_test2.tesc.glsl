#version 430 core
//#extension GL_EXT_control_flow_attributes : enable

#include "internal/_vs_common.glsl"

layout (vertices = 1) out;

layout(location = 0) in vec3 g_vtx_pos_cs[];
layout(location = 1) in vec2 g_vtx_uvs_cs[];
layout(location = 2) in vec3 g_vtx_norm_cs[];
layout(location = 3) in vec3 g_vtx_tangent_cs[];
layout(location = 4) in vec3 g_vtx_sh_uvs_cs[][4];

struct OutputPatch {
    vec3 aVertexPos_B030;
    vec3 aVertexPos_B021;
    vec3 aVertexPos_B012;
    vec3 aVertexPos_B003;
    vec3 aVertexPos_B102;
    vec3 aVertexPos_B201;
    vec3 aVertexPos_B300;
    vec3 aVertexPos_B210;
    vec3 aVertexPos_B120;
    vec3 aVertexPos_B111;
    vec2 g_in_vtx_uvs[3];
    vec3 aVertexNormal[3];
    vec3 aVertexTangent[3];
    //vec3 aVertexShUVs[3][4];
};

layout(location = 0) out patch OutputPatch oPatch;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

float GetTessLevel(float Distance0, float Distance1) {
    float AvgDistance = (Distance0 + Distance1) / 1.0;
    return min((2.0 * 1024.0) / (AvgDistance * AvgDistance), 8.0);
}

vec3 ProjectToPlane(vec3 Point, vec3 PlanePoint, vec3 PlaneNormal) {
    vec3 v = Point - PlanePoint;
    float Len = dot(v, PlaneNormal);
    vec3 d = Len * PlaneNormal;
    return (Point - d);
}

void CalcPositions() {
    // The original vertices stay the same
    oPatch.aVertexPos_B030 = g_vtx_pos_cs[0];
    oPatch.aVertexPos_B003 = g_vtx_pos_cs[1];
    oPatch.aVertexPos_B300 = g_vtx_pos_cs[2];

    // Edges are names according to the opposing vertex
    vec3 EdgeB300 = oPatch.aVertexPos_B003 - oPatch.aVertexPos_B030;
    vec3 EdgeB030 = oPatch.aVertexPos_B300 - oPatch.aVertexPos_B003;
    vec3 EdgeB003 = oPatch.aVertexPos_B030 - oPatch.aVertexPos_B300;

    // Generate two midpoints on each edge
    oPatch.aVertexPos_B021 = oPatch.aVertexPos_B030 + EdgeB300 / 3.0;
    oPatch.aVertexPos_B012 = oPatch.aVertexPos_B030 + EdgeB300 * 2.0 / 3.0;
    oPatch.aVertexPos_B102 = oPatch.aVertexPos_B003 + EdgeB030 / 3.0;
    oPatch.aVertexPos_B201 = oPatch.aVertexPos_B003 + EdgeB030 * 2.0 / 3.0;
    oPatch.aVertexPos_B210 = oPatch.aVertexPos_B300 + EdgeB003 / 3.0;
    oPatch.aVertexPos_B120 = oPatch.aVertexPos_B300 + EdgeB003 * 2.0 / 3.0;

    // Project each midpoint on the plane defined by the nearest vertex and its normal
    oPatch.aVertexPos_B021 = ProjectToPlane(oPatch.aVertexPos_B021, oPatch.aVertexPos_B030,
                                            oPatch.aVertexNormal[0]);
    oPatch.aVertexPos_B012 = ProjectToPlane(oPatch.aVertexPos_B012, oPatch.aVertexPos_B003,
                                            oPatch.aVertexNormal[1]);
    oPatch.aVertexPos_B102 = ProjectToPlane(oPatch.aVertexPos_B102, oPatch.aVertexPos_B003,
                                            oPatch.aVertexNormal[1]);
    oPatch.aVertexPos_B201 = ProjectToPlane(oPatch.aVertexPos_B201, oPatch.aVertexPos_B300,
                                            oPatch.aVertexNormal[2]);
    oPatch.aVertexPos_B210 = ProjectToPlane(oPatch.aVertexPos_B210, oPatch.aVertexPos_B300,
                                            oPatch.aVertexNormal[2]);
    oPatch.aVertexPos_B120 = ProjectToPlane(oPatch.aVertexPos_B120, oPatch.aVertexPos_B030,
                                            oPatch.aVertexNormal[0]);

    // Handle the center
    vec3 Center = (oPatch.aVertexPos_B003 + oPatch.aVertexPos_B030 + oPatch.aVertexPos_B300) / 3.0;
    oPatch.aVertexPos_B111 = (oPatch.aVertexPos_B021 + oPatch.aVertexPos_B012 + oPatch.aVertexPos_B102 +
                          oPatch.aVertexPos_B201 + oPatch.aVertexPos_B210 + oPatch.aVertexPos_B120) / 6.0;
    oPatch.aVertexPos_B111 += (oPatch.aVertexPos_B111 - Center) / 2.0;
}

void main(void) {
    for (int i = 0; i < 3; i++) {
        oPatch.g_in_vtx_uvs[i] = g_vtx_uvs_cs[i];
        oPatch.aVertexNormal[i] = g_vtx_norm_cs[i];
        oPatch.aVertexTangent[i] = g_vtx_tangent_cs[i];
        //oPatch.aVertexShUVs[i][0] = g_vtx_sh_uvs_cs[i][0];
        //oPatch.aVertexShUVs[i][1] = g_vtx_sh_uvs_cs[i][1];
        //oPatch.aVertexShUVs[i][2] = g_vtx_sh_uvs_cs[i][2];
        //oPatch.aVertexShUVs[i][3] = g_vtx_sh_uvs_cs[i][3];
    }

    CalcPositions();

    float EyeToVertexDistance0 = distance(g_shrd_data.cam_pos_and_exp.xyz, g_vtx_pos_cs[0]);
    float EyeToVertexDistance1 = distance(g_shrd_data.cam_pos_and_exp.xyz, g_vtx_pos_cs[1]);
    float EyeToVertexDistance2 = distance(g_shrd_data.cam_pos_and_exp.xyz, g_vtx_pos_cs[2]);

    gl_TessLevelOuter[0] = GetTessLevel(EyeToVertexDistance1, EyeToVertexDistance2);
    gl_TessLevelOuter[1] = GetTessLevel(EyeToVertexDistance2, EyeToVertexDistance0);
    gl_TessLevelOuter[2] = GetTessLevel(EyeToVertexDistance0, EyeToVertexDistance1);
    gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) * (1.0 / 3.0);
}
