R"(
#version 310 es
#extension GL_EXT_texture_cube_map_array : enable

#ifdef GL_ES
    precision highp float;
#endif
        
layout(binding = 0) uniform mediump samplerCubeArray s_texture;
layout(binding = 1) uniform mediump sampler2D s_rand;

layout(location = 1) uniform float src_layer;
layout(location = 2) uniform int iteration;

in vec2 aVertexUVs_;

out vec4 outColor;

vec3 RGBMDecode(vec4 rgbm) {
    return 6.0 * rgbm.rgb * rgbm.a;
}

#define M_PI 3.1415926535897932384626433832795

void main() {
    int x = int(aVertexUVs_.x);
    int y = int(aVertexUVs_.y);

    vec2 rand2d = texelFetch(s_rand, ivec2(x % 8, y), 0).xy + texelFetch(s_rand, ivec2(iteration % 8, iteration / 8), 0).xy;
    rand2d = fract(rand2d);

    float theta = acos(-1.0 + 2.0 * rand2d.x);
    float phi = 2.0 * M_PI * rand2d.y;

    vec3 rand_vec = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

    const float Y0 = 0.282094806; // sqrt(1.0 / (4.0 * PI))
    const float Y1 = 0.488602519; // sqrt(3.0 / (4.0 * PI))

    vec4 sh;

    sh.x = Y0;
    sh.y = Y1 * rand_vec.y;
    sh.z = Y1 * rand_vec.z;
    sh.w = Y1 * rand_vec.x;

    vec3 color = RGBMDecode(textureLod(s_texture, vec4(rand_vec, src_layer), 0.0));

    if (x < 8) {
        // red channel
        sh *= color.r;
    } else if (x < 16) {
        // green channel
        sh *= color.g;
    } else {
        // blue channel
        sh *= color.b;
    }

    outColor = sh;
}
)"