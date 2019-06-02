R"(
#version 310 es
#extension GL_EXT_texture_buffer : enable

layout(location = )" AS_STR(REN_VTX_POS_LOC) R"() in vec3 aVertexPosition;

layout(binding = )" AS_STR(REN_INST_BUF_SLOT) R"() uniform highp samplerBuffer instances_buffer;

layout(location = )" AS_STR(REN_U_M_MATRIX_LOC) R"() uniform mat4 uViewProjMatrix;
layout(location = )" AS_STR(REN_U_INSTANCES_LOC) R"() uniform int uInstanceIndices[8];

void main() {
    int instance = uInstanceIndices[gl_InstanceID];

    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

    gl_Position = uViewProjMatrix * MMatrix * vec4(aVertexPosition, 1.0);
} 
)"