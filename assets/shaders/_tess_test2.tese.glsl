#version 320 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

#include "internal/_vs_common.glsl"

layout(triangles, fractional_odd_spacing, ccw) in;
//layout(triangles, equal_spacing, ccw) in;

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

layout(location = 0) in patch OutputPatch oPatch;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out highp vec3 g_vtx_pos;
layout(location = 1) out mediump vec2 g_vtx_uvs;
layout(location = 2) out mediump vec3 g_vtx_normal;
layout(location = 3) out mediump vec3 g_vtx_tangent;
layout(location = 4) out highp vec3 g_vtx_sh_uvs[4];
#else
out highp vec3 g_vtx_pos;
out mediump vec2 g_vtx_uvs;
out mediump vec3 g_vtx_normal;
out mediump vec3 g_vtx_tangent;
out highp vec3 g_vtx_sh_uvs[4];
#endif

layout(binding = BIND_MAT_TEX3) uniform sampler2D g_bump_tex;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

void main(void) {
    g_vtx_uvs = gl_TessCoord[0] * oPatch.g_in_vtx_uvs[0] + gl_TessCoord[1] * oPatch.g_in_vtx_uvs[1] + gl_TessCoord[2] * oPatch.g_in_vtx_uvs[2];
    g_vtx_normal = gl_TessCoord[0] * oPatch.aVertexNormal[0] + gl_TessCoord[1] * oPatch.aVertexNormal[1] + gl_TessCoord[2] * oPatch.aVertexNormal[2];
    g_vtx_tangent = gl_TessCoord[0] * oPatch.aVertexTangent[0] + gl_TessCoord[1] * oPatch.aVertexTangent[1] + gl_TessCoord[2] * oPatch.aVertexTangent[2];

    //g_vtx_sh_uvs[0] = gl_TessCoord[0] * oPatch.aVertexShUVs[0][0] + gl_TessCoord[1] * oPatch.aVertexShUVs[1][0] + gl_TessCoord[2] * oPatch.aVertexShUVs[2][0];
    //g_vtx_sh_uvs[1] = gl_TessCoord[0] * oPatch.aVertexShUVs[0][1] + gl_TessCoord[1] * oPatch.aVertexShUVs[1][1] + gl_TessCoord[2] * oPatch.aVertexShUVs[2][1];
    //g_vtx_sh_uvs[2] = gl_TessCoord[0] * oPatch.aVertexShUVs[0][2] + gl_TessCoord[1] * oPatch.aVertexShUVs[1][2] + gl_TessCoord[2] * oPatch.aVertexShUVs[2][2];
    //g_vtx_sh_uvs[3] = gl_TessCoord[0] * oPatch.aVertexShUVs[0][3] + gl_TessCoord[1] * oPatch.aVertexShUVs[1][3] + gl_TessCoord[2] * oPatch.aVertexShUVs[2][3];

    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;
    float w = gl_TessCoord.z;

    float u_pow2 = u * u;
    float v_pow2 = v * v;
    float w_pow2 = w * w;
    float u_pow3 = u_pow2 * u;
    float v_pow3 = v_pow2 * v;
    float w_pow3 = w_pow2 * w;

    g_vtx_pos = oPatch.aVertexPos_B300 * w_pow3 +
                  oPatch.aVertexPos_B030 * u_pow3 +
                  oPatch.aVertexPos_B003 * v_pow3 +
                  oPatch.aVertexPos_B210 * 3.0 * w_pow2 * u +
                  oPatch.aVertexPos_B120 * 3.0 * w * u_pow2 +
                  oPatch.aVertexPos_B201 * 3.0 * w_pow2 * v +
                  oPatch.aVertexPos_B021 * 3.0 * u_pow2 * v +
                  oPatch.aVertexPos_B102 * 3.0 * w * v_pow2 +
                  oPatch.aVertexPos_B012 * 3.0 * u * v_pow2 +
                  oPatch.aVertexPos_B111 * 6.0 * w * u * v;

    gl_Position = g_shrd_data.view_proj_no_translation * vec4(g_vtx_pos - g_shrd_data.cam_pos_and_gamma.xyz, 1.0);
}
