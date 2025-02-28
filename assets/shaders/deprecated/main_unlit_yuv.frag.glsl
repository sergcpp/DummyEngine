#version 430 core
//#extension GL_EXT_control_flow_attributes : enable
#if !defined(VULKAN)
#extension GL_ARB_bindless_texture : enable
#endif

#include "../internal/_fs_common.glsl"
#include "../internal/texturing_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(BINDLESS_TEXTURES)
layout(binding = BIND_MAT_TEX0) uniform sampler2D g_mat0_tex;
layout(binding = BIND_MAT_TEX1) uniform sampler2D g_mat1_tex;
#endif // BINDLESS_TEXTURES

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(location = 4) in vec2 g_vtx_uvs;
#if defined(BINDLESS_TEXTURES)
    layout(location = 8) in flat TEX_HANDLE g_mat0_tex;
    layout(location = 9) in flat TEX_HANDLE g_mat1_tex;
#endif // BINDLESS_TEXTURES

layout(location = LOC_OUT_COLOR) out vec4 g_out_color;
layout(location = LOC_OUT_NORM) out vec4 g_out_normal;

void main() {
    vec3 col_yuv;
    col_yuv.x = texture(SAMPLER2D(g_mat0_tex), g_vtx_uvs).r;
    col_yuv.yz = texture(SAMPLER2D(g_mat1_tex), g_vtx_uvs).rg;
    col_yuv += vec3(-0.0627451017, -0.501960814, -0.501960814);

    vec3 col_rgb;
    col_rgb.r = dot(col_yuv, vec3(1.164,  0.000,  1.596));
    col_rgb.g = dot(col_yuv, vec3(1.164, -0.391, -0.813));
    col_rgb.b = dot(col_yuv, vec3(1.164,  2.018,  0.000));

    g_out_color = vec4(SRGBToLinear(col_rgb), 1.0);
    g_out_normal = vec4(0.0);
}
