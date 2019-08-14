R"(#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = 0) uniform sampler2D s_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

vec4 RGBMEncode(vec3 color) {
    vec4 rgbm;
    color *= 1.0 / 4.0;
    rgbm.a = clamp(max(max(color.r, color.g), max(color.b, 1e-6)), 0.0, 1.0);
    rgbm.a = ceil(rgbm.a * 255.0) / 255.0;
    rgbm.rgb = color / rgbm.a;
    return rgbm;
}

void main() {
    outColor = RGBMEncode(texelFetch(s_texture, ivec2(aVertexUVs_), 0).rgb);
}
)"