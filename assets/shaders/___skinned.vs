
/*
ATTRIBUTES
    aVertexPosition : 0
    aVertexNormal : 1
    aVertexUVs : 2
    aVertexIndices : 3
    aVertexWeights : 4
UNIFORMS
    uMVPMatrix : 0
    uMVMatrix : 1
    uMPalette[0] : 2
*/

attribute vec3 aVertexPosition;
attribute vec2 aVertexUVs;
attribute vec3 aVertexNormal;

attribute vec4 aVertexIndices;
attribute vec4 aVertexWeights;

uniform mat4 uMVPMatrix;
uniform mat4 uMVMatrix;
uniform mat4 uMPalette[MAX_GPU_BONES];

varying vec3 aVertexPosition_;
varying vec2 aVertexUVs_;
varying vec3 aVertexNormal_;

void main(void) {
    aVertexUVs_ = aVertexUVs;

    mat4 mat = uMPalette[int(aVertexIndices.x)] * aVertexWeights.x;
#if 0
    vec4 vtx_indices = aVertexIndices;
    vec4 vtx_weights = aVertexWeights;
    for(int i = 0; i < 3; i++) {
        vtx_indices = vtx_indices.yzwx;
        vtx_weights = vtx_weights.yzwx;
        if(aVertexWeights.x > 0.0) {
            mat = mat + uMPalette[int(vtx_indices.x)] * vtx_weights.x;
        }
    }
#endif
    aVertexPosition_ = vec3((uMVMatrix * mat) * vec4(aVertexPosition, 1.0));
    gl_Position = (uMVPMatrix * mat) * vec4(aVertexPosition, 1.0);
    aVertexNormal_ = normalize(vec3((uMVMatrix * mat) * vec4(aVertexNormal, 0.0)));
}
