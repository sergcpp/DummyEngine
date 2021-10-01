
#include "_common.glsl"

const uint RTGeoProbeBits = 0xff;
const uint RTGeoLightmappedBit = (1u << 8u);

struct RTGeoInstance {
	uint indices_start;
	uint vertices_start;
	uint material_index;
	uint flags;
	vec4 lmap_transform;
};

vec3 EvalSHIrradiance_NonLinear(vec3 dir, vec4 sh_r, vec4 sh_g, vec4 sh_b) {
    vec3 R1_len = vec3(length(sh_r.yzw), length(sh_g.yzw), length(sh_b.yzw));
    vec3 R1_inv_len = mix(vec3(0.0), vec3(1.0) / R1_len, step(vec3(FLT_EPS), R1_len));
    vec3 R0 = vec3(sh_r.x, sh_g.x, sh_b.x);

    vec3 q = 0.5 * (vec3(1.0) + vec3(dot(dir.yzx, sh_r.yzw), dot(dir.yzx, sh_g.yzw),
                                     dot(dir.yzx, sh_b.yzw)) * R1_inv_len);
    vec3 p = vec3(1.0) + 2.0 * R1_len / R0;
    vec3 a = (vec3(1.0) - R1_len / R0) / (vec3(1.0) + R1_len / R0);

    return R0 * (a + (vec3(1.0) - a) * (p + vec3(1.0)) * pow(q, p));
}

float pow5(float x) {
    return (x * x) * (x * x) * x;
}

vec3 FresnelSchlickRoughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow5(1.0 - cos_theta);
}