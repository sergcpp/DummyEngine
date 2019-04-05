R"(
#version 310 es

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

layout(location = )" AS_STR(REN_VTX_POS_LOC) R"() in vec3 aVertexPosition;

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    mat4 uSunShadowMatrix[4];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPos;
    vec4 uResGamma;
};

layout(location = )" AS_STR(REN_U_M_MATRIX_LOC) R"() uniform mat4 uMMatrix;

out vec3 aVertexPos_;

void main() {
    vec3 vertex_position_ws = (uMMatrix * vec4(aVertexPosition, 1.0)).xyz;
    aVertexPos_ = vertex_position_ws;

    gl_Position = uViewProjMatrix * uMMatrix * vec4(aVertexPosition, 1.0);
} 
)"