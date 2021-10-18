#version 310 es
#extension GL_EXT_texture_cube_map_array : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif
        
layout(binding = 0) uniform mediump samplerCubeArray s_texture;
layout(binding = 1) uniform mediump sampler2D s_rand;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) float src_layer;
                        int iteration;
};
#else
layout(location = 1) uniform float src_layer;
layout(location = 2) uniform int iteration;
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
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

    vec4 sh;

    sh.x = 1.0;
    sh.y = rand_vec.y;
    sh.z = rand_vec.z;
    sh.w = rand_vec.x;

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
