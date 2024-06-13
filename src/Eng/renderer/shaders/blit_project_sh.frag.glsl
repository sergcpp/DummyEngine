#version 430 core
#extension GL_EXT_texture_cube_map_array : enable

layout(binding = 0) uniform samplerCubeArray g_tex;
layout(binding = 1) uniform sampler2D g_rand;

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    layout(offset = 16) float src_layer;
                        int iteration;
};
#else
layout(location = 1) uniform float src_layer;
layout(location = 2) uniform int iteration;
#endif

#if defined(VULKAN)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
}

#define M_PI 3.1415926535897932384626433832795

void main() {
    int x = int(g_vtx_uvs.x);
    int y = int(g_vtx_uvs.y);

    vec2 rand2d = texelFetch(g_rand, ivec2(x % 8, y), 0).xy + texelFetch(g_rand, ivec2(iteration % 8, iteration / 8), 0).xy;
    rand2d = fract(rand2d);

    float theta = acos(-1.0 + 2.0 * rand2d.x);
    float phi = 2.0 * M_PI * rand2d.y;

    vec3 rand_vec = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

    vec4 sh;

    sh.x = 1.0;
    sh.y = rand_vec.y;
    sh.z = rand_vec.z;
    sh.w = rand_vec.x;

    vec3 color = RGBMDecode(textureLod(g_tex, vec4(rand_vec, src_layer), 0.0));

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

    g_out_color = sh;
}
