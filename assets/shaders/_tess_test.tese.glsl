#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "internal/_vs_common.glsl"
#include "internal/_texturing.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(triangles, fractional_odd_spacing, ccw) in;
//layout(triangles, equal_spacing, ccw) in;

LAYOUT(location = 0) in highp vec3 g_vtx_pos_es[];
LAYOUT(location = 1) in mediump vec2 g_vtx_uvs_es[];
LAYOUT(location = 2) in mediump vec3 g_vtx_norm_es[];
LAYOUT(location = 3) in mediump vec3 g_vtx_tangent_es[];
LAYOUT(location = 4) in highp vec3 g_vtx_sh_uvs_es[][4];
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 12) out flat TEX_HANDLE g_bump_texture;
#endif // BINDLESS_TEXTURES

LAYOUT(location = 0) out highp vec3 g_vtx_pos;
LAYOUT(location = 1) out mediump vec2 g_vtx_uvs;
LAYOUT(location = 2) out mediump vec3 g_vtx_normal;
LAYOUT(location = 3) out mediump vec3 g_vtx_tangent;
LAYOUT(location = 4) out highp vec3 g_vtx_sh_uvs[4];
LAYOUT(location = 8) out lowp float g_tex_height;

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX3_SLOT) uniform sampler2D g_bump_texture;
#endif // BINDLESS_TEXTURES

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

void main(void) {
    g_vtx_pos = gl_TessCoord[0] * g_vtx_pos_es[0] + gl_TessCoord[1] * g_vtx_pos_es[1] + gl_TessCoord[2] * g_vtx_pos_es[2];
    g_vtx_uvs = gl_TessCoord[0] * g_vtx_uvs_es[0] + gl_TessCoord[1] * g_vtx_uvs_es[1] + gl_TessCoord[2] * g_vtx_uvs_es[2];
    g_vtx_normal = gl_TessCoord[0] * g_vtx_norm_es[0] + gl_TessCoord[1] * g_vtx_norm_es[1] + gl_TessCoord[2] * g_vtx_norm_es[2];
    g_vtx_tangent = gl_TessCoord[0] * g_vtx_tangent_es[0] + gl_TessCoord[1] * g_vtx_tangent_es[1] + gl_TessCoord[2] * g_vtx_tangent_es[2];

    g_vtx_sh_uvs[0] = gl_TessCoord[0] * g_vtx_sh_uvs_es[0][0] + gl_TessCoord[1] * g_vtx_sh_uvs_es[1][0] + gl_TessCoord[2] * g_vtx_sh_uvs_es[2][0];
    g_vtx_sh_uvs[1] = gl_TessCoord[0] * g_vtx_sh_uvs_es[0][1] + gl_TessCoord[1] * g_vtx_sh_uvs_es[1][1] + gl_TessCoord[2] * g_vtx_sh_uvs_es[2][1];
    g_vtx_sh_uvs[2] = gl_TessCoord[0] * g_vtx_sh_uvs_es[0][2] + gl_TessCoord[1] * g_vtx_sh_uvs_es[1][2] + gl_TessCoord[2] * g_vtx_sh_uvs_es[2][2];
    g_vtx_sh_uvs[3] = gl_TessCoord[0] * g_vtx_sh_uvs_es[0][3] + gl_TessCoord[1] * g_vtx_sh_uvs_es[1][3] + gl_TessCoord[2] * g_vtx_sh_uvs_es[2][3];

    float k = clamp((32.0 - distance(g_shrd_data.cam_pos_and_gamma.xyz, g_vtx_pos)) / 32.0, 0.0, 1.0);
    k *= k;
    k = 1.0;
    //float k = gl_TessLevelInner[0] / 64.0;

    //g_vtx_pos.y += 4.0 * sin(g_vtx_pos.x * 0.1);
    g_tex_height = 0.5 * texture(SAMPLER2D(g_bump_texture), g_vtx_uvs).r * k;
    g_vtx_pos += 1.0 * 0.05 * normalize(g_vtx_normal) * g_tex_height * k;

    gl_Position = g_shrd_data.view_proj_matrix * vec4(g_vtx_pos, 1.0);
}
