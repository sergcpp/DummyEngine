R"(
#version 310 es

layout(location = )" AS_STR(REN_VTX_POS_LOC) R"() in vec2 aVertexPosition;
layout(location = )" AS_STR(REN_VTX_UV1_LOC) R"() in vec2 aVertexUVs;

layout(location = 0) uniform vec4 uTransform;

out vec2 aVertexUVs_;

void main() {
    aVertexUVs_ = aVertexUVs;
    gl_Position = vec4(aVertexPosition, 0.5, 1.0);
} 
)"