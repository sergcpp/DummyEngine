#version 320 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

#include "internal/_vs_common.glsl"

layout (vertices = 3) out;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 g_vtx_pos_cs[];
layout(location = 1) in mediump vec2 g_vtx_uvs_cs[];
layout(location = 2) in mediump vec3 g_vtx_norm_cs[];
layout(location = 3) in mediump vec3 g_vtx_tangent_cs[];
layout(location = 4) in highp vec3 g_vtx_sh_uvs_cs[][4];
#else
in highp vec3 g_vtx_pos_cs[];
in mediump vec2 g_vtx_uvs_cs[];
in mediump vec3 g_vtx_norm_cs[];
in mediump vec3 g_vtx_tangent_cs[];
in highp vec3 g_vtx_sh_uvs_cs[][4];
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out highp vec3 g_vtx_pos_es[];
layout(location = 1) out mediump vec2 g_vtx_uvs_es[];
layout(location = 2) out mediump vec3 g_vtx_norm_es[];
layout(location = 3) out mediump vec3 g_vtx_tangent_es[];
layout(location = 4) out highp vec3 g_vtx_sh_uvs_es[][4];
#else
out highp vec3 g_vtx_pos_es[];
out mediump vec2 g_vtx_uvs_es[];
out mediump vec3 g_vtx_norm_es[];
out mediump vec3 g_vtx_tangent_es[];
out highp vec3 g_vtx_sh_uvs_es[][4];
#endif

layout (binding = 0, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

float GetTessLevel(float Distance0, float Distance1) {
    float AvgDistance = (Distance0 + Distance1) / 1.0;
    return min((32.0 * 1024.0) / (AvgDistance * AvgDistance), 64.0);
}

void main(void) {
    g_vtx_pos_es[gl_InvocationID] = g_vtx_pos_cs[gl_InvocationID];
    g_vtx_uvs_es[gl_InvocationID] = g_vtx_uvs_cs[gl_InvocationID];
    g_vtx_norm_es[gl_InvocationID] = g_vtx_norm_cs[gl_InvocationID];
    g_vtx_tangent_es[gl_InvocationID] = g_vtx_tangent_cs[gl_InvocationID];
    g_vtx_sh_uvs_es[gl_InvocationID][0] = g_vtx_sh_uvs_cs[gl_InvocationID][0];
    g_vtx_sh_uvs_es[gl_InvocationID][1] = g_vtx_sh_uvs_cs[gl_InvocationID][1];
    g_vtx_sh_uvs_es[gl_InvocationID][2] = g_vtx_sh_uvs_cs[gl_InvocationID][2];
    g_vtx_sh_uvs_es[gl_InvocationID][3] = g_vtx_sh_uvs_cs[gl_InvocationID][3];

#if 1
    float EyeToVertexDistance0 = distance(g_shrd_data.cam_pos_and_gamma.xyz, g_vtx_pos_es[0]);
    float EyeToVertexDistance1 = distance(g_shrd_data.cam_pos_and_gamma.xyz, g_vtx_pos_es[1]);
    float EyeToVertexDistance2 = distance(g_shrd_data.cam_pos_and_gamma.xyz, g_vtx_pos_es[2]);

    gl_TessLevelOuter[0] = GetTessLevel(EyeToVertexDistance1, EyeToVertexDistance2);
    gl_TessLevelOuter[1] = GetTessLevel(EyeToVertexDistance2, EyeToVertexDistance0);
    gl_TessLevelOuter[2] = GetTessLevel(EyeToVertexDistance0, EyeToVertexDistance1);
    gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) * (1.0 / 3.0);
#else
    const float MaxTesselation = 64.0;
    const float ZoomFactor = 4.0;

    vec3 v0 = g_shrd_data.cam_pos_and_gamma.xyz - g_vtx_pos_es[0];
    vec3 v1 = g_shrd_data.cam_pos_and_gamma.xyz - g_vtx_pos_es[1];
    vec3 v2 = g_shrd_data.cam_pos_and_gamma.xyz - g_vtx_pos_es[2];

    float d0 = length(v0);
    float d1 = length(v1);
    float d2 = length(v2);

    float t0 = (1.0 - abs(dot(g_vtx_norm_es[0], v0) / d0)) * GetTessLevel(d1, d2);
    float t1 = (1.0 - abs(dot(g_vtx_norm_es[1], v1) / d1)) * GetTessLevel(d2, d0);
    float t2 = (1.0 - abs(dot(g_vtx_norm_es[2], v2) / d2)) * GetTessLevel(d0, d1);

    gl_TessLevelOuter[0] = t0;
    gl_TessLevelOuter[1] = t1;
    gl_TessLevelOuter[2] = t2;
    gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) * (1.0 / 3.0);
#endif
}
