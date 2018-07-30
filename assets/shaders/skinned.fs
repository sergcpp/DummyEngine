#ifdef GL_ES
	precision lowp float;
#endif

varying vec2 aVertexUVs_;
varying vec3 aVertexNormal_;

void main(void) {
	float k = aVertexNormal_.z * aVertexNormal_.z;
	float f = clamp(0.1 + 1 * pow((1 - k), 8), 0, 1);
	gl_FragColor = vec4(aVertexUVs_ * 0.001, 0, 0) + 0.001*vec4(aVertexNormal_*0.5 + vec3(0.5, 0.5, 0.5), 1.0);
	vec3 c1 = vec3(1, 0, 0);
	vec3 c2 = vec3(0, 1, 0);
	gl_FragColor.rgb += mix(c1, c2, k);
}
