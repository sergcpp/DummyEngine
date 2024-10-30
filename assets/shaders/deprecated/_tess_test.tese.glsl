#version 430 core
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif

#include "../internal/_vs_common.glsl"
#include "../internal/texturing_common.glsl"

layout(triangles, fractional_odd_spacing, ccw) in;
//layout(triangles, equal_spacing, ccw) in;

layout(location = 0) in vec3 g_vtx_pos_es[];
layout(location = 1) in vec2 g_vtx_uvs_es[];
layout(location = 2) in vec3 g_vtx_norm_es[];
layout(location = 3) in vec3 g_vtx_tangent_es[];
layout(location = 4) in vec3 g_vtx_sh_uvs_es[][4];
#if defined(BINDLESS_TEXTURES)
    layout(location = 12) out flat TEX_HANDLE g_bump_tex;
#endif // BINDLESS_TEXTURES

layout(location = 0) out vec3 g_vtx_pos;
layout(location = 1) out vec2 g_vtx_uvs;
layout(location = 2) out vec3 g_vtx_normal;
layout(location = 3) out vec3 g_vtx_tangent;
layout(location = 4) out vec3 g_vtx_sh_uvs[4];
layout(location = 8) out float g_tex_height;

#if !defined(BINDLESS_TEXTURES)
layout(binding = BIND_MAT_TEX3) uniform sampler2D g_bump_tex;
#endif // BINDLESS_TEXTURES

layout (binding = 0, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

void main() {
    g_vtx_pos = gl_TessCoord[0] * g_vtx_pos_es[0] + gl_TessCoord[1] * g_vtx_pos_es[1] + gl_TessCoord[2] * g_vtx_pos_es[2];
    g_vtx_uvs = gl_TessCoord[0] * g_vtx_uvs_es[0] + gl_TessCoord[1] * g_vtx_uvs_es[1] + gl_TessCoord[2] * g_vtx_uvs_es[2];
    g_vtx_normal = gl_TessCoord[0] * g_vtx_norm_es[0] + gl_TessCoord[1] * g_vtx_norm_es[1] + gl_TessCoord[2] * g_vtx_norm_es[2];
    g_vtx_tangent = gl_TessCoord[0] * g_vtx_tangent_es[0] + gl_TessCoord[1] * g_vtx_tangent_es[1] + gl_TessCoord[2] * g_vtx_tangent_es[2];

    g_vtx_sh_uvs[0] = gl_TessCoord[0] * g_vtx_sh_uvs_es[0][0] + gl_TessCoord[1] * g_vtx_sh_uvs_es[1][0] + gl_TessCoord[2] * g_vtx_sh_uvs_es[2][0];
    g_vtx_sh_uvs[1] = gl_TessCoord[0] * g_vtx_sh_uvs_es[0][1] + gl_TessCoord[1] * g_vtx_sh_uvs_es[1][1] + gl_TessCoord[2] * g_vtx_sh_uvs_es[2][1];
    g_vtx_sh_uvs[2] = gl_TessCoord[0] * g_vtx_sh_uvs_es[0][2] + gl_TessCoord[1] * g_vtx_sh_uvs_es[1][2] + gl_TessCoord[2] * g_vtx_sh_uvs_es[2][2];
    g_vtx_sh_uvs[3] = gl_TessCoord[0] * g_vtx_sh_uvs_es[0][3] + gl_TessCoord[1] * g_vtx_sh_uvs_es[1][3] + gl_TessCoord[2] * g_vtx_sh_uvs_es[2][3];

    float k = clamp((32.0 - distance(g_shrd_data.cam_pos_and_exp.xyz, g_vtx_pos)) / 32.0, 0.0, 1.0);
    k *= k;
    k = 1.0;
    //float k = gl_TessLevelInner[0] / 64.0;

    //g_vtx_pos.y += 4.0 * sin(g_vtx_pos.x * 0.1);
    g_tex_height = 0.5 * texture(SAMPLER2D(g_bump_tex), g_vtx_uvs).r * k;
    g_vtx_pos += 1.0 * 0.05 * normalize(g_vtx_normal) * g_tex_height * k;

    gl_Position = g_shrd_data.clip_from_world * vec4(g_vtx_pos, 1.0);
}
