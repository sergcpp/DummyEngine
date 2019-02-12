R"(
#version 300 es

/*
UNIFORMS
	uMVPMatrix : 0
*/

layout(location = 0) in vec3 aVertexPosition;

uniform mat4 uMVPMatrix;

void main() {
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
)"