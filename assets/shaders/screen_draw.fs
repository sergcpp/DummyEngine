#ifdef GL_ES
precision mediump float;
#endif

/*
UNIFORMS
	positions : 3
	normals : 4
	light_pos : 5
	light_col : 6
*/

uniform vec3 light_pos;
uniform vec3 light_col;

uniform sampler2D positions;
uniform sampler2D normals;

varying vec4 aVertexPosition_;

void main(void) {
	vec3 pos = texture2D(positions, 0.5 * (aVertexPosition_.xy / aVertexPosition_.w) + 0.5).xyz;
	vec3 normal = texture2D(normals, 0.5 * (aVertexPosition_.xy / aVertexPosition_.w) + 0.5).xyz;

	normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
	
	vec3 v = light_pos - pos;
	float dist = sqrt(dot(v, v));

	gl_FragColor.xyz = light_col * max(dot(v / dist, normal), 0.0) / (1.0 + dist * dist * dist * dist * dist * dist);

	//gl_FragColor.xyz *= 0.001;
	//gl_FragColor.xyz += 0.5 * normal + 0.5;
}
