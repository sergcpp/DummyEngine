R"(
#version 310 es

layout(location = 0) in vec2 aVertexPosition;
layout(location = 3) in vec2 aVertexUVs;

out vec2 aVertexUVs_;

void main() {
    aVertexUVs_ = aVertexUVs;
    gl_Position = vec4(aVertexPosition, 0.5, 1.0);
} 
)"