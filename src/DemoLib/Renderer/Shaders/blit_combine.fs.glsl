R"(
#version 310 es
#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = 0) uniform sampler2D s_texture;
layout(binding = 1) uniform sampler2D s_blured_texture;
layout(location = 13) uniform vec2 uTexSize;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform float exposure;

in vec2 aVertexUVs_;

out vec4 outColor;

vec3 Unch2Tonemap(vec3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    const float W = 11.2;

    return ((x * (A * x + vec3(C * B)) + vec3(D * E)) / (x * ( A * x + vec3(B)) + vec3(D * F))) - vec3(E / F);
}

void main() {
    vec2 norm_uvs = aVertexUVs_ / uTexSize;

    vec3 c0 = texelFetch(s_texture, ivec2(aVertexUVs_), 0).xyz;
    vec3 c1 = 0.1 * texture(s_blured_texture, norm_uvs).xyz;
            
    c0 += c1;

    c0 = Unch2Tonemap(exposure * c0);

    const float W = 11.2;
    vec3 white = 1.0 / Unch2Tonemap(vec3(W));

    c0 = pow(c0 * white, vec3(1.0/gamma));

    outColor = vec4(c0, 1.0);
}
)"