#version 430

#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = 0) uniform sampler2D g_tex;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

vec4 RGBMEncode(vec3 color) {
    vec4 rgbm;
    color *= 1.0 / 4.0;
    rgbm.a = clamp(max(max(color.r, color.g), max(color.b, 1e-6)), 0.0, 1.0);
    rgbm.a = ceil(rgbm.a * 255.0) / 255.0;
    rgbm.rgb = color / rgbm.a;
    return rgbm;
}

void main() {
    g_out_color = RGBMEncode(texelFetch(g_tex, ivec2(g_vtx_uvs), 0).rgb);
}
