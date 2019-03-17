R"(
#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = 0) uniform samplerCube env_texture;

layout(location = 0) uniform vec3 camera_pos;

in vec3 aVertexPos_;

layout(location = 0) out vec4 outColor;
layout(location = 2) out vec4 outSpecular;

vec3 RGBMDecode(vec4 rgbm) {
    return 6.0 * rgbm.rgb * rgbm.a;
}

void main() {
    vec3 view_dir_ws = normalize(aVertexPos_ - camera_pos);

    outColor.rgb = clamp(RGBMDecode(texture(env_texture, view_dir_ws)), vec3(0.0), vec3(16.0));
    outColor.a = 1.0;
    outSpecular = vec4(0.0, 0.0, 0.0, 1.0);
}
)"