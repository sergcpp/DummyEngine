#version 310 es
#extension GL_EXT_texture_cube_map_array : enable

#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = 0) uniform mediump samplerCubeArray s_texture;
layout(location = 1) uniform float src_layer;
layout(location = 2) uniform int src_face;
layout(location = 3) uniform highp float roughness;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

#define M_PI 3.1415926535897932384626433832795

vec3 gen_cubemap_coord(in vec2 txc, in int face) {
    vec3 v;
    switch(face) {
        case 0: v = vec3( 1.0,   -txc.x,  txc.y); break; // +X
        case 1: v = vec3(-1.0,   -txc.x, -txc.y); break; // -X
        case 2: v = vec3(-txc.y,  1.0,    txc.x); break; // +Y
        case 3: v = vec3(-txc.y, -1.0,   -txc.x); break; // -Y
        case 4: v = vec3(-txc.y, -txc.x,  1.0); break;   // +Z
        case 5: v = vec3( txc.y, -txc.x, -1.0); break;   // -Z
    }
    return normalize(v);
}

float RadicalInverse_VdC(highp uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    highp float ret = float(bits);
    return ret * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley2D(uint i, uint N) {
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, float roughness, vec3 N) {
	float a = roughness * roughness;

	float Phi = 2.0 * M_PI * Xi.x;
	float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float SinTheta = sqrt(1.0 - CosTheta * CosTheta);

	vec3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
	vec3 TangentX = normalize(cross(up, N));
	vec3 TangentY = cross(N, TangentX);
	// Tangent to world space
	return TangentX * H.x + TangentY * H.y + N * H.z;
}

float DistributionGGX(float NdotH, float a) {
    float a2     = a * a;
    float NdotH2 = NdotH*NdotH;
	
    float nom    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = M_PI * denom * denom;
	
    return nom / denom;
}

vec4 RGBMEncode(vec3 color) {
    vec4 rgbm;
    color *= 1.0 / 4.0;
    rgbm.a = clamp(max(max(color.r, color.g), max(color.b, 1e-6)), 0.0, 1.0);
    rgbm.a = ceil(rgbm.a * 255.0) / 255.0;
    rgbm.rgb = color / rgbm.a;
    return rgbm;
}

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
}

vec3 PrefilterEnvMap(float roughness, vec3 r) {
	vec3 n = r;
	vec3 v = r;

	vec3 res_col = vec3(0.0);
	float res_weight = 0.0;

    const uint SampleCount = 1024u;

	for (uint i = 0u; i < SampleCount; i++) {
		vec2 rand2d = Hammersley2D(i, SampleCount);
		vec3 h = ImportanceSampleGGX(rand2d, roughness, n);
		vec3 l = 2.0 * dot(v, h) * h - v;

		float n_dot_l = clamp(dot(n, l), 0.0, 1.0);
		if (n_dot_l > 0.0) {
			float n_dot_h = dot(n, h);

			float D   = DistributionGGX(n_dot_h, roughness);
			highp float pdf = (D * n_dot_h / (4.0 * dot(h, v))) + 0.0001;

			const highp float resolution = 512.0; // resolution of source cubemap (per face)
			const highp float sa_texel = 10.0 * 4.0 * M_PI / (6.0 * resolution * resolution); // multiplied by 10 to avoid precision problems
			highp float sa_sample = 0.1 / (float(SampleCount) * pdf + 0.0001);

			float mip_level = roughness == 0.0 ? 0.0 : 0.5 * log2(sa_sample / sa_texel);

			vec4 col = textureLod(s_texture, vec4(l, src_layer), mip_level);
			res_col += RGBMDecode(col) * n_dot_l;
			res_weight += n_dot_l;
		}
	}

	return res_col / res_weight;
}

void main() {
	vec3 r  = gen_cubemap_coord(aVertexUVs_, src_face);
	outColor = RGBMEncode(PrefilterEnvMap(roughness, r));
}
