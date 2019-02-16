R"(
#version 310 es

#ifdef GL_ES
	precision mediump float;
#endif

layout(binding = 0) uniform samplerCube env_texture;

layout(location = 0) uniform vec3 camera_pos;

in vec3 aVertexPos_;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outSpecular;

void main() {
	vec3 view_dir_ws = normalize(aVertexPos_ - camera_pos);

    outColor = texture(env_texture, view_dir_ws);
outColor.r = 1.0;
	outSpecular = vec4(0.0, 0.0, 0.0, 1.0);
}
)"