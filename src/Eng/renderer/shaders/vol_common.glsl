#ifndef VOL_COMMON_GLSL
#define VOL_COMMON_GLSL

#include "principled_common.glsl"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

float vol_slice_distance(const int z, const float offset, const int froxel_res_z) {
    return g_shrd_data.clip_info.y * pow(g_shrd_data.clip_info.z / g_shrd_data.clip_info.y, (float(z) + offset) / float(froxel_res_z));
}

vec3 froxel_to_uvw(const ivec3 coord, const float z_offset, const ivec3 froxel_res) {
    const float z = vol_slice_distance(coord.z, z_offset, froxel_res.z);
    return vec3((vec2(coord.xy) + 0.5) / vec2(froxel_res.xy), DelinearizeDepth(z, g_shrd_data.clip_info));
}

vec3 cs_to_uvw(const vec2 ndc, const float lin_depth) {
    return vec3(ndc * 0.5 + 0.5,
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

vec3 EvaluateLightSource_Vol(const _light_item_t litem, const vec3 P, const vec3 I, const float anisotropy) {
    const uint type = floatBitsToUint(litem.col_and_type.w) & LIGHT_TYPE_BITS;
    const vec3 from_light = normalize(P - litem.pos_and_radius.xyz);
    const float _dot = -dot(from_light, litem.dir_and_spot.xyz);
    const float _angle = approx_acos(_dot);
    if (type == LIGHT_TYPE_SPHERE && _angle > litem.dir_and_spot.w) {
        return vec3(0.0);
    } else if (type != LIGHT_TYPE_LINE && _angle > (0.5 * M_PI)) {
        // Single-sided
        return vec3(0.0);
    }

    vec3 ret = vec3(0.0);

    const float sqr_dist = dot(litem.pos_and_radius.xyz - P, litem.pos_and_radius.xyz - P);
    const vec3 L = normalize(litem.pos_and_radius.xyz - P), H = normalize(I + L);

    if (type == LIGHT_TYPE_SPHERE && ENABLE_SPHERE_LIGHT != 0) {
        const float brightness_mul = litem.pos_and_radius.w * litem.pos_and_radius.w;
        ret += brightness_mul * litem.col_and_type.xyz / (M_PI * max(0.001, sqr_dist));
        if (litem.v_and_blend.w > 0.0) {
            ret *= saturate((litem.dir_and_spot.w - _angle) / sqr(litem.v_and_blend.w));
        }
    } else if (type == LIGHT_TYPE_RECT && ENABLE_RECT_LIGHT != 0) {
        vec3 points[4];
        points[0] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz + litem.v_and_blend.xyz;
        points[1] = litem.pos_and_radius.xyz + litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[2] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz - litem.v_and_blend.xyz;
        points[3] = litem.pos_and_radius.xyz - litem.u_and_reg.xyz + litem.v_and_blend.xyz;
        const float solid_angle = RectangleSolidAngle(P, points);
        ret += litem.col_and_type.xyz * solid_angle / (4.0 * M_PI);
    } else if (type == LIGHT_TYPE_DISK && ENABLE_DISK_LIGHT != 0) {
        const float inv_solid_angle = sqr_dist / (length(litem.u_and_reg.xyz) * length(litem.v_and_blend.xyz));
        const float front_dot_l = saturate(dot(litem.dir_and_spot.xyz, L));
        ret += litem.col_and_type.xyz * saturate(front_dot_l / (1.0 + inv_solid_angle)) / 4.0;
    } else if (type == LIGHT_TYPE_LINE && ENABLE_LINE_LIGHT != 0) {
        const float brightness_mul = 0.004 * litem.pos_and_radius.w;
        const vec3 L0 = litem.pos_and_radius.xyz + litem.v_and_blend.xyz - P;
        const vec3 L1 = litem.pos_and_radius.xyz - litem.v_and_blend.xyz - P;
        const vec2 len_sqr = vec2(dot(L0, L0), dot(L1, L1));
        const vec2 inv_len = inversesqrt(len_sqr);
        const vec2 len = len_sqr * inv_len;
        ret += brightness_mul * litem.col_and_type.xyz / max(0.001, 0.5 * (len.x * len.y + dot(L0, L1)));
    }

    ret *= HenyeyGreenstein(dot(L, I), anisotropy);

    return ret;
}

#endif // VOL_COMMON_GLSL