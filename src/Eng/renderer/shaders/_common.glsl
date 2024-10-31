#ifndef COMMON_GLSL
#define COMMON_GLSL

#include "Constants.inl"
#include "Types.h"

#define M_PI 3.1415926535897932384626433832795
#define GOLDEN_RATIO 1.61803398875
#define SQRT_2 1.41421356237

#define HALF_MAX 65504.0

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1e-15

// limit for 10- and 11-bit float value (https://www.khronos.org/opengl/wiki/Small_Float_Formats)
#define SMALL_FLT_MAX 6.5e4


#define saturate(x) min(max((x), 0.0), 1.0)
#define rcp(x) (1.0 / (x))
#define positive_rcp(x) (1.0 / max((x), FLT_MIN))
#define sqr(x) ((x) * (x))

#define min3(x, y, z) min((x), min((y), (z)))
#define max3(x, y, z) max((x), max((y), (z)))

#define deg2rad(x) ((x) * M_PI / 180.0)
#define rad2deg(x) ((x) * 180.0 / M_PI)

#define length2(x) dot(x, x)

#define LinearizeDepth(z, clip_info) \
    (((clip_info)[0] / ((1.0 - (z)) * ((clip_info)[1] - (clip_info)[2]) + (clip_info)[2])))

#define DelinearizeDepth(z, clip_info) \
    (1.0 - ((clip_info)[0] / (z) - (clip_info)[2]) / ((clip_info)[1] - (clip_info)[2]))

float approx_acos(float x) { // max error is 0.000068f
    float negate = float(x < 0);
    x = abs(x);
    float ret = -0.0187293;
    ret = ret * x;
    ret = ret + 0.0742610;
    ret = ret * x;
    ret = ret - 0.2121144;
    ret = ret * x;
    ret = ret + 1.5707288;
    ret = ret * sqrt(1.0 - saturate(x));
    ret = ret - 2.0 * negate * ret;
    return negate * M_PI + ret;
}

vec3 heatmap(float t) {
    vec3 r = vec3(t) * 2.1 - vec3(1.8, 1.14, 0.3);
    return vec3(1.0) - r * r;
}

float hsum(vec3 v) {
    return v.x + v.y + v.z;
}

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 rgbe_to_rgb(const vec4 rgbe) {
    const float f = exp2(255.0 * rgbe.w - 128.0);
    return rgbe.xyz * f;
}

vec3 rotate_xz(vec3 dir, float angle) {
    const float x = dir.x * cos(angle) - dir.z * sin(angle);
    const float z = dir.x * sin(angle) + dir.z * cos(angle);
    return vec3(x, dir.y, z);
}

float from_unit_to_sub_uvs(const float u, const float resolution) {
    return (u + 0.5 / resolution) * (resolution / (resolution + 1.0));
}
float from_sub_uvs_to_unit(const float u, const float resolution) {
    return (u - 0.5 / resolution) * (resolution / (resolution - 1.0));
}

float linstep(const float smin, const float smax, const float x) {
    return saturate((x - smin) / (smax - smin));
}

uint hash(uint x) {
    // finalizer from murmurhash3
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

float construct_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    const float  f = uintBitsToFloat(m);   // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

// Octahedron packing for unit vectors - xonverts a 3D unit vector to a 2D vector with [0; 1] range
// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
// [Cigolle 2014, "A Survey of Efficient Representations for Independent Unit Vectors"]
vec2 PackUnitVector(const vec3 v) {
    const vec3 t = v / (abs(v.x) + abs(v.y) + abs(v.z));
    const vec2 _sign = vec2(t.x >= 0.0 ? 1.0 : -1.0,
                            t.y >= 0.0 ? 1.0 : -1.0);
    vec2 a = t.z >= 0.0 ? t.xy : (vec2(1.0) - abs(t.yx)) * _sign;
    a = saturate(a * 0.5 + vec2(0.5));

    return a.xy;
}

vec3 UnpackUnitVector(const vec2 p) {
    const vec2 t = p * 2.0 - vec2(1.0);

    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3(t.x, t.y, 1.0 - abs(t.x) - abs(t.y));
    const float a = saturate(-n.z);
    n.x += n.x >= 0.0 ? -a : a;
    n.y += n.y >= 0.0 ? -a : a;

    return normalize(n);
}

vec4 PackNormalAndRoughness(vec3 N, float roughness) {
    vec4 p;

#if USE_OCT_PACKED_NORMALS == 1
    p.xy = PackUnitVector(N);
    p.z = roughness;
    p.w = 0;
#else
    p.xyz = N;

    // Best fit
    const float m = max(abs(N.x), max(abs(N.y), abs(N.z)));
    p.xyz *= positive_rcp(m);

    p.xyz = p.xyz * 0.5 + 0.5;
    p.w = roughness;
#endif

    return p;
}

uint PackNormalAndRoughnessNew(const vec3 N, const float roughness, const vec2 rand) {
    vec3 p;

    p.xy = PackUnitVector(N);
    p.z = roughness;
    p *= vec3(4095.0, 4095.0, 255.0);
    p.xy += rand;

    return uint(p.z) | (uint(p.y) << 8) | (uint(p.x) << 20);
}

uint PackNormalAndRoughnessNew(const vec3 N, const float roughness) {
    return PackNormalAndRoughnessNew(N, roughness, vec2(0.0));
}

vec4 UnpackNormalAndRoughness(vec4 p) {
    vec4 r;

#if USE_OCT_PACKED_NORMALS == 1
    r.xyz = UnpackUnitVector(p.xy);
    r.w = p.z;
#else
    p.xyz = p.xyz * 2.0 - 1.0;
    r.xyz = p.xyz;
    r.w = p.w;
#endif
    r.xyz = normalize(r.xyz);

    return r;
}

vec4 UnpackNormalAndRoughness(const uint f) {
    vec3 p;
    p.x = float((f >> 20) & 4095u);
    p.y = float((f >> 8) & 4095u);
    p.z = float((f >> 0) & 255u);
    p /= vec3(4095.0, 4095.0, 255.0);

    vec4 r;
    r.xyz = UnpackUnitVector(p.xy);
    r.w = p.z;
    r.xyz = normalize(r.xyz);

    return r;
}

uint PackMaterialParams(const vec4 params0, const vec4 params1) {
    uvec4 uparams0 = uvec4(round(params0 * 15.0));
    uvec4 uparams1 = uvec4(round(params1 * 15.0));

    uparams0 <<= uvec4(0, 4, 8, 12);
    uparams1 <<= uvec4(16, 20, 24, 28);

    return uparams0.x | uparams0.y | uparams0.z | uparams0.w |
           uparams1.x | uparams1.y | uparams1.z | uparams1.w;
}

void UnpackMaterialParams(uint _packed, out vec4 params0, out vec4 params1) {
    const uvec4 uparams0 = uvec4(_packed >> 0u, _packed >> 4u, _packed >> 8u, _packed >> 12u) & uvec4(0xF);
    const uvec4 uparams1 = uvec4(_packed >> 16u, _packed >> 20u, _packed >> 24u, _packed >> 28u) & uvec4(0xF);

    params0 = vec4(uparams0) / 15.0;
    params1 = vec4(uparams1) / 15.0;
}

vec3 YCoCg_to_RGB(vec4 col) {
    const float scale = (col.b * (255.0 / 8.0)) + 1.0;
    const float Y = col.a;
    const float Co = (col.r - (0.5 * 256.0 / 255.0)) / scale;
    const float Cg = (col.g - (0.5 * 256.0 / 255.0)) / scale;

    vec3 col_rgb;
    col_rgb.r = Y + Co - Cg;
    col_rgb.g = Y + Cg;
    col_rgb.b = Y - Co - Cg;

    return saturate(col_rgb);
}

vec3 normalize_len(const vec3 v, out float len) {
    return (v / (len = length(v)));
}

float lum(vec3 color) {
    return dot(vec3(0.212671, 0.715160, 0.715160), color);
}

vec3 limit_intensity(vec3 color, const float limit) {
    const float sum = color.x + color.y + color.z;
    if (sum > 3.0 * limit) {
        color *= (3.0 * limit / sum);
    }
    return color;
}

vec3 compress_hdr(const vec3 val, const float pre_exposure) {
    return clamp(val * pre_exposure, vec3(0.0), vec3(HALF_MAX - 1.0));
}

float sanitize(const float val) {
    return isnan(val) ? 0.0 : val;
}

vec3 sanitize(const vec3 col) {
    return vec3(sanitize(col.x), sanitize(col.y), sanitize(col.z));
}

vec4 sanitize(const vec4 col) {
    return vec4(sanitize(col.x), sanitize(col.y), sanitize(col.z), sanitize(col.w));
}

vec3 TransformFromClipSpace(const mat4 world_from_clip, vec4 pos_cs) {
#if defined(VULKAN)
    pos_cs.y = -pos_cs.y;
#endif // VULKAN
    const vec4 pos_ws = world_from_clip * pos_cs;
    return pos_ws.xyz / pos_ws.w;
}

vec2 RotateVector(vec4 rotator, vec2 v) { return v.x * rotator.xz + v.y * rotator.yw; }
vec4 CombineRotators(vec4 r1, vec4 r2 ) { return r1.xyxy * r2.xxzz + r1.zwzw * r2.yyww; }

float hash(vec2 v) {
    return fract(1.0e4 * sin(17.0 * v.x + 0.1 * v.y) * (0.1 + abs(sin(13.0 * v.y + v.x))));
}

float hash3D(vec3 v) {
    return hash(vec2(hash(v.xy), v.z));
}

// Ray Tracing Gems II, Listing 49-1
vec3 ReconstructViewPosition(vec2 uv, vec4 cam_frustum, float view_z, float is_ortho) {
    vec3 p;
    p.xy = uv.xy * cam_frustum.zw + cam_frustum.xy;

    p.xy *= view_z * (1.0 - abs(is_ortho)) + is_ortho;
    p.z = view_z;

    return p;
}

vec3 ReconstructViewPosition_YFlip(vec2 uv, vec4 cam_frustum, float view_z, float is_ortho) {
#if defined(VULKAN)
    uv = vec2(uv.x, 1 - uv.y);
#endif
    return ReconstructViewPosition(uv, cam_frustum, view_z, is_ortho);
}

float PixelRadiusToWorld(float unproject, float is_ortho, float pixel_radius, float view_z) {
     return pixel_radius * unproject * mix(view_z, 1.0, abs(is_ortho));
}

mat2x3 CreateTangentVectors(vec3 normal) {
	vec3 up = abs(normal.z) < 0.99999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);

	mat2x3 tangents;

	tangents[0] = normalize(cross(up, normal));
	tangents[1] = cross(normal, tangents[0]);

	return tangents;
}

vec3 MapToCone(vec2 u, vec3 n, float radius) {
	vec2 offset = 2.0 * u - vec2(1.0);

	if (offset.x == 0.0 && offset.y == 0.0) {
		return n;
	}

	float theta, r;

	if (abs(offset.x) > abs(offset.y)) {
		r = offset.x;
		theta = 0.25 * M_PI * (offset.y / offset.x);
	} else {
		r = offset.y;
		theta = 0.5 * M_PI * (1.0 - 0.5 * (offset.x / offset.y));
	}

	vec2 uv = vec2(radius * r * cos(theta), radius * r * sin(theta));

	mat2x3 tangents = CreateTangentVectors(n);

	return n + uv.x * tangents[0] + uv.y * tangents[1];
}

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct ProbeItem {
    vec4 pos_and_radius;
    vec4 unused_and_layer;
    vec4 sh_coeffs[3];
};

struct EllipsItem {
    vec4 pos_and_radius;
    vec4 axis_and_perp;
};

struct SharedData {
    mat4 view_from_world, clip_from_view, clip_from_world, prev_view_from_world, prev_clip_from_world;
    mat4 world_from_view, view_from_clip, world_from_clip, delta_matrix;
    mat4 rt_clip_from_world;
    ShadowMapRegion shadowmap_regions[MAX_SHADOWMAPS_TOTAL];
    vec4 sun_dir, sun_col, sun_col_point, sun_col_point_sh, env_col, taa_info, frustum_info;
    vec4 clip_info, rt_clip_info, cam_pos_and_exp, prev_cam_pos;
    vec4 res_and_fres, transp_params_and_time;
    ivec4 ires_and_ifres;
    vec4 wind_scroll, wind_scroll_prev;
    uvec4 item_counts;
    vec4 ambient_hack;
    ProbeVolume probe_volumes[PROBE_VOLUMES_COUNT];
    uvec4 portals[MAX_PORTALS_TOTAL / 4];
    ProbeItem probes[MAX_PROBES_TOTAL];
    EllipsItem ellipsoids[MAX_ELLIPSES_TOTAL];
    AtmosphereParams atmosphere;
};

struct MaterialData {
    uint texture_indices[MAX_TEX_PER_MATERIAL];
    uint _pad[2];
    vec4 params[4];
};

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
}

uint ReverseBits4(uint x) {
    x = ((x & 0x5u) << 1u) | (( x & 0xAu) >> 1u);
    x = ((x & 0x3u) << 2u) | (( x & 0xCu) >> 2u);
    return x;
}

// https://en.wikipedia.org/wiki/Ordered_dithering
// RESULT: [0; 15]
uint Bayer4x4ui(const uvec2 sample_pos, const uint frame) {
    const uvec2 sample_pos_wrap = sample_pos & 3u;
    const uint a = 2068378560u * (1u - (sample_pos_wrap.x >> 1u)) + 1500172770u * (sample_pos_wrap.x >> 1u);
    const uint b = (sample_pos_wrap.y + ((sample_pos_wrap.x & 1u) << 2u)) << 2u;

    uint sample_offset = frame;
#if 1 // BAYER_REVERSEBITS
    sample_offset = ReverseBits4(sample_offset);
#endif

    return ((a >> b) + sample_offset) & 0xFu;
}

// RESULT: [0; 1)
float Bayer4x4(uvec2 sample_pos, uint frame) {
    uint bayer = Bayer4x4ui(sample_pos, frame);
    return float(bayer) / 16.0;
}

float copysign(const float val, const float sign) {
    return sign < 0.0 ? -abs(val) : abs(val);
}

bool bbox_test(vec3 o, vec3 inv_d, float t, vec3 bbox_min, vec3 bbox_max) {
    float low = inv_d.x * (bbox_min[0] - o.x);
    float high = inv_d.x * (bbox_max[0] - o.x);
    float tmin = min(low, high);
    float tmax = max(low, high);

    low = inv_d.y * (bbox_min[1] - o.y);
    high = inv_d.y * (bbox_max[1] - o.y);
    tmin = max(tmin, min(low, high));
    tmax = min(tmax, max(low, high));

    low = inv_d.z * (bbox_min[2] - o.z);
    high = inv_d.z * (bbox_max[2] - o.z);
    tmin = max(tmin, min(low, high));
    tmax = min(tmax, max(low, high));
    tmax *= 1.00000024;

    return tmin <= tmax && tmin <= t && tmax > 0.0;
}

bool bbox_test_fma(vec3 inv_d, vec3 neg_inv_d_o, float t, vec3 bbox_min, vec3 bbox_max) {
    float low = fma(inv_d.x, bbox_min.x, neg_inv_d_o.x);
    float high = fma(inv_d.x, bbox_max.x, neg_inv_d_o.x);
    float tmin = min(low, high);
    float tmax = max(low, high);

    low = fma(inv_d.y, bbox_min.y, neg_inv_d_o.y);
    high = fma(inv_d.y, bbox_max.y, neg_inv_d_o.y);
    tmin = max(tmin, min(low, high));
    tmax = min(tmax, max(low, high));

    low = fma(inv_d.z, bbox_min.z, neg_inv_d_o.z);
    high = fma(inv_d.z, bbox_max.z, neg_inv_d_o.z);
    tmin = max(tmin, min(low, high));
    tmax = min(tmax, max(low, high));
    tmax *= 1.00000024;

    return tmin <= tmax && tmin <= t && tmax > 0.0;
}

bool bbox_test(const vec3 p, const vec3 bbox_min, const vec3 bbox_max) {
    return p.x >= bbox_min.x && p.x <= bbox_max.x &&
           p.y >= bbox_min.y && p.y <= bbox_max.y &&
           p.z >= bbox_min.z && p.z <= bbox_max.z;
}

bool bbox_test(vec3 inv_d, vec3 neg_inv_d_o, float t, vec3 bbox_min, vec3 bbox_max, out float dist) {
    float low = fma(inv_d.x, bbox_min.x, neg_inv_d_o.x);
    float high = fma(inv_d.x, bbox_max.x, neg_inv_d_o.x);
    float tmin = min(low, high);
    float tmax = max(low, high);

    low = fma(inv_d.y, bbox_min.y, neg_inv_d_o.y);
    high = fma(inv_d.y, bbox_max.y, neg_inv_d_o.y);
    tmin = max(tmin, min(low, high));
    tmax = min(tmax, max(low, high));

    low = fma(inv_d.z, bbox_min.z, neg_inv_d_o.z);
    high = fma(inv_d.z, bbox_max.z, neg_inv_d_o.z);
    tmin = max(tmin, min(low, high));
    tmax = min(tmax, max(low, high));
    tmax *= 1.00000024;

    dist = tmin;

    return tmin <= tmax && tmin <= t && tmax > 0.0;
}

#endif // COMMON_GLSL