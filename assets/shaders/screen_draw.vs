
/*
ATTRIBUTES
	aVertexPosition : 0
	aVertexUVs : 1
*/

attribute vec2 aVertexPosition;
attribute vec2 aVertexUVs;

varying vec2 aVertexUVs_;

void main(void) {
    gl_Position = vec4(aVertexPosition, 0, 1);
    aVertexUVs_ = aVertexUVs;
}
