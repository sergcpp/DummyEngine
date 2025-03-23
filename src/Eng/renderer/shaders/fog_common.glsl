#ifndef FOG_COMMON_GLSL
#define FOG_COMMON_GLSL

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

float fog_slice_distance(const int z, const float offset, const int froxel_res_z) {
    return g_shrd_data.clip_info.y * pow(g_shrd_data.clip_info.z / g_shrd_data.clip_info.y, (float(z) + offset) / float(froxel_res_z));
}

vec3 froxel_to_uvw(const ivec3 coord, const float z_offset, const ivec3 froxel_res) {
    const float z = fog_slice_distance(coord.z, z_offset, froxel_res.z);
    return vec3((vec2(coord.xy) + 0.5) / vec2(froxel_res.xy), DelinearizeDepth(z, g_shrd_data.clip_info));
}

vec3 cs_to_uvw(const vec3 ndc, const float lin_depth) {
    return vec3(ndc.xy * 0.5 + 0.5,
                log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3]);
}

float HenyeyGreenstein(const float mu, const float g) {
    return (1.0 - g * g) / (pow(1.0 + g * g - 2.0 * g * mu, 1.5) * 4.0 * M_PI);
}

// https://gist.github.com/Fewes/59d2c831672040452aa77da6eaab2234
vec4 SampleTricubic(const sampler3D tex, const vec3 coord, const vec3 texture_size) {
	// Shift the coordinate from [0,1] to [-0.5, texture_size-0.5]
	const vec3 coord_grid = coord * texture_size - 0.5;
	const vec3 index = floor(coord_grid);
	const vec3 fraction = coord_grid - index;
	const vec3 one_frac = 1.0 - fraction;

	const vec3 w0 = (1.0 / 6.0) * one_frac * one_frac * one_frac;
	const vec3 w1 = (2.0 / 3.0) - 0.5 * fraction * fraction * (2.0 - fraction);
	const vec3 w2 = (2.0 / 3.0) - 0.5 * one_frac * one_frac * (2.0 - one_frac);
	const vec3 w3 = (1.0 / 6.0) * fraction * fraction * fraction;

	const vec3 g0 = w0 + w1;
	const vec3 g1 = w2 + w3;
	const vec3 mult = 1.0 / texture_size;
	const vec3 h0 = mult * ((w1 / g0) - 0.5 + index); // h0 = w1/g0 - 1, move from [-0.5, texture_size-0.5] to [0,1]
	const vec3 h1 = mult * ((w3 / g1) + 1.5 + index); // h1 = w3/g1 + 1, move from [-0.5, texture_size-0.5] to [0,1]

	// Fetch the eight linear interpolations
	// Weighting and fetching is interleaved for performance and stability reasons
	vec4 tex000 = textureLod(tex, h0, 0.0);
	vec4 tex100 = textureLod(tex, vec3(h1.x, h0.y, h0.z), 0.0);
	tex000 = mix(tex100, tex000, g0.x); // Weigh along the x-direction

	vec4 tex010 = textureLod(tex, vec3(h0.x, h1.y, h0.z), 0.0);
	vec4 tex110 = textureLod(tex, vec3(h1.x, h1.y, h0.z), 0.0);
	tex010 = mix(tex110, tex010, g0.x); // Weigh along the x-direction
	tex000 = mix(tex010, tex000, g0.y); // Weigh along the y-direction

	vec4 tex001 = textureLod(tex, vec3(h0.x, h0.y, h1.z), 0.0);
	vec4 tex101 = textureLod(tex, vec3(h1.x, h0.y, h1.z), 0.0);
	tex001 = mix(tex101, tex001, g0.x); // Weigh along the x-direction

	vec4 tex011 = textureLod(tex, vec3(h0.x, h1.y, h1.z), 0.0);
	vec4 tex111 = textureLod(tex, vec3(h1), 0.0);
	tex011 = mix(tex111, tex011, g0.x); // Weigh along the x-direction
	tex001 = mix(tex011, tex001, g0.y); // Weigh along the y-direction

	return mix(tex001, tex000, g0.z); // Weigh along the z-direction
}

#endif // FOG_COMMON_GLSL