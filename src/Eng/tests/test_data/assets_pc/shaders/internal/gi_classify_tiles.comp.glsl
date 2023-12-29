#version 430
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_shuffle : enable
#extension GL_KHR_shader_subgroup_vote : enable
#endif

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#line 0
#ifndef _CS_COMMON_GLSL
#define _CS_COMMON_GLSL

#line 0

#line 0

// Resolution of frustum item grid
#define REN_GRID_RES_X 16
#define REN_GRID_RES_Y 8
#define REN_GRID_RES_Z 24

// Attribute location
#define REN_VTX_POS_LOC 0
#define REN_VTX_NOR_LOC 1
#define REN_VTX_TAN_LOC 2
#define REN_VTX_UV1_LOC 3
#define REN_VTX_AUX_LOC 4

#define REN_VTX_PRE_LOC 5 // vertex position in previous frame

// Texture binding
#define REN_MAT_TEX0_SLOT   0
#define REN_MAT_TEX1_SLOT   1
#define REN_MAT_TEX2_SLOT   2
#define REN_MAT_TEX3_SLOT   3
#define REN_MAT_TEX4_SLOT   4
#define REN_MAT_TEX5_SLOT   5
#define REN_SHAD_TEX_SLOT   6
#define REN_LMAP_SH_SLOT    7
#define REN_DECAL_TEX_SLOT  11
#define REN_SSAO_TEX_SLOT   12
#define REN_BRDF_TEX_SLOT   13
#define REN_LIGHT_BUF_SLOT  14
#define REN_DECAL_BUF_SLOT  15
#define REN_CELLS_BUF_SLOT  16
#define REN_ITEMS_BUF_SLOT  17
#define REN_INST_BUF_SLOT   18
#define REN_INST_INDICES_BUF_SLOT 19
#define REN_ENV_TEX_SLOT    20
#define REN_NOISE_TEX_SLOT 21
#define REN_CONE_RT_LUT_SLOT 22

#define REN_SET_SCENETEXTURES 1

#define REN_MATERIALS_SLOT 25
#define REN_BINDLESS_TEX_SLOT 0 // shares slot with material slot 0

#define REN_BASE0_TEX_SLOT 0
#define REN_BASE1_TEX_SLOT 1
#define REN_BASE2_TEX_SLOT 2

#define REN_REFL_DEPTH_TEX_SLOT 0
#define REN_REFL_NORM_TEX_SLOT  1
#define REN_REFL_SPEC_TEX_SLOT  2
#define REN_REFL_PREV_TEX_SLOT  3
#define REN_REFL_SSR_TEX_SLOT   4
#define REN_REFL_ENV_TEX_SLOT   5
#define REN_REFL_BRDF_TEX_SLOT  6
#define REN_REFL_DEPTH_LOW_TEX_SLOT 7

#define REN_U_BASE_INSTANCE_LOC 2

#define REN_UB_SHARED_DATA_LOC  23
#define REN_UB_UNIF_PARAM_LOC    24

// Shader output location
#define REN_OUT_COLOR_INDEX 0
#define REN_OUT_NORM_INDEX  1
#define REN_OUT_SPEC_INDEX  2
#define REN_OUT_VELO_INDEX  3
#define REN_OUT_ALBEDO_INDEX 0

// Shadow resolution
#define REN_SHAD_RES_PC         8192
#define REN_SHAD_RES_ANDROID    4096

#define REN_SHAD_RES REN_SHAD_RES_PC

// Shadow cascades definition
#define REN_SHAD_CASCADE0_DIST      10.0
#define REN_SHAD_CASCADE0_SAMPLES   16
#define REN_SHAD_CASCADE1_DIST      24.0
#define REN_SHAD_CASCADE1_SAMPLES   8
#define REN_SHAD_CASCADE2_DIST      48.0
#define REN_SHAD_CASCADE2_SAMPLES   4
#define REN_SHAD_CASCADE3_DIST      96.0
#define REN_SHAD_CASCADE3_SAMPLES   4
#define REN_SHAD_CASCADE_SOFT       1

#define REN_MAX_OBJ_COUNT			4194304
#define REN_MAX_TEX_COUNT           262144

#define REN_MAX_TEX_PER_MATERIAL    6
#define REN_MAX_MATERIALS           262144

#define REN_MAX_INSTANCES_TOTAL     262144
#define REN_MAX_SHADOWMAPS_TOTAL    32
#define REN_MAX_PROBES_TOTAL        32
#define REN_MAX_ELLIPSES_TOTAL      64
#define REN_MAX_SKIN_XFORMS_TOTAL   65536
#define REN_MAX_SKIN_REGIONS_TOTAL  262144
#define REN_MAX_SKIN_VERTICES_TOTAL 1048576
#define REN_MAX_SHAPE_KEYS_TOTAL    1024

#define REN_MAX_SHADOW_BATCHES      262144
#define REN_MAX_MAIN_BATCHES        262144

#define REN_DECALS_BUF_STRIDE		7

#define REN_USE_OCT_PACKED_NORMALS	1

#define REN_CELLS_COUNT (REN_GRID_RES_X * REN_GRID_RES_Y * REN_GRID_RES_Z)

#define REN_MAX_LIGHTS_PER_CELL 255
#define REN_MAX_DECALS_PER_CELL 255
#define REN_MAX_PROBES_PER_CELL 8
#define REN_MAX_ELLIPSES_PER_CELL 16
#define REN_MAX_ITEMS_PER_CELL  255

#define REN_MAX_LIGHTS_TOTAL 4096
#define REN_MAX_DECALS_TOTAL 4096
#define REN_MAX_ITEMS_TOTAL int(1u << 16u)

#define REN_MAX_RT_GEO_INSTANCES	16384
#define REN_MAX_RT_OBJ_INSTANCES	4096
#define REN_MAX_RT_TLAS_NODES		8192 // (4096 + 2048 + 1024 + ...)

#define REN_OIT_DISABLED            0
#define REN_OIT_MOMENT_BASED        1
#define REN_OIT_WEIGHTED_BLENDED    2

#define REN_OIT_MOMENT_RENORMALIZE  1

#define REN_OIT_MODE REN_OIT_DISABLED

#define FLT_EPS 0.0000001

#line 0

#define M_PI 3.1415926535897932384626433832795
#define GOLDEN_RATIO 1.61803398875

#define FLT_MIN 1e-15

// limit for 10- and 11-bit float value (https://www.khronos.org/opengl/wiki/Small_Float_Formats)
#define SMALL_FLT_MAX 6.5e4

#define saturate(x) min(max((x), 0.0), 1.0)
#define rcp(x) (1.0 / (x))
#define positive_rcp(x) (1.0 / max((x), FLT_MIN))

#define min3(x, y, z) min((x), min((y), (z)))
#define max3(x, y, z) max((x), max((y), (z)))

#define deg2rad(x) ((x) * M_PI / 180.0)
#define rad2deg(x) ((x) * 180.0 / M_PI)

#define length2(x) dot(x, x)

#define LinearizeDepth(z, clip_info) \
    (((clip_info)[0] / ((z) * ((clip_info)[1] - (clip_info)[2]) + (clip_info)[2])))

#define DelinearizeDepth(z, clip_info) \
    (((clip_info)[0] / (z) - (clip_info)[2]) / ((clip_info)[1] - (clip_info)[2]))

// Octahedron packing for unit vectors - xonverts a 3D unit vector to a 2D vector with [0; 1] range
// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
// [Cigolle 2014, "A Survey of Efficient Representations for Independent Unit Vectors"]
vec2 PackUnitVector(vec3 v) {
    vec3 t = v / (abs(v.x) + abs(v.y) + abs(v.z));
    vec2 _sign = vec2(t.x >= 0.0 ? 1.0 : -1.0,
                      t.y >= 0.0 ? 1.0 : -1.0);
    vec2 a = t.z >= 0.0 ? t.xy : (vec2(1.0) - abs(t.yx)) * _sign;
    a = saturate(a * 0.5 + vec2(0.5));

    return a.xy;
}

vec3 UnpackUnitVector(vec2 p) {
    vec2 t = p * 2.0 - vec2(1.0);

    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3(t.x, t.y, 1.0 - abs(t.x) - abs(t.y));
    float a = saturate(-n.z);
    n.x += n.x >= 0.0 ? -a : a;
    n.y += n.y >= 0.0 ? -a : a;

    return normalize(n);
}

vec4 PackNormalAndRoughness(vec3 N, float roughness) {
    vec4 p;

#if REN_USE_OCT_PACKED_NORMALS == 1
    p.xy = PackUnitVector(N);
    p.z = roughness;
    p.w = 0;
#else
    p.xyz = N;

    // Best fit
    float m = max(abs(N.x), max(abs(N.y), abs(N.z)));
    p.xyz *= positive_rcp(m);

    p.xyz = p.xyz * 0.5 + 0.5;
    p.w = roughness;
#endif

    return p;
}

vec4 UnpackNormalAndRoughness(vec4 p) {
    vec4 r;

#if REN_USE_OCT_PACKED_NORMALS == 1
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

uint PackMaterialParams(vec4 params0, vec4 params1) {
    uvec4 uparams0 = uvec4(round(params0 * 15.0));
    uvec4 uparams1 = uvec4(round(params1 * 15.0));

    uparams0 <<= uvec4(0, 4, 8, 12);
    uparams1 <<= uvec4(16, 20, 24, 28);

    return uparams0.x | uparams0.y | uparams0.z | uparams0.w |
           uparams1.x | uparams1.y | uparams1.z | uparams1.w;
}

void UnpackMaterialParams(uint _packed, out vec4 params0, out vec4 params1) {
    uvec4 uparams0 = uvec4(_packed >> 0u, _packed >> 4u, _packed >> 8u, _packed >> 12u) & uvec4(0xF);
    uvec4 uparams1 = uvec4(_packed >> 16u, _packed >> 20u, _packed >> 24u, _packed >> 28u) & uvec4(0xF);

    params0 = vec4(uparams0) / 15.0;
    params1 = vec4(uparams1) / 15.0;
}

vec3 YCoCg_to_RGB(vec4 col) {
    float scale = (col.b * (255.0 / 8.0)) + 1.0;
    float Y = col.a;
    float Co = (col.r - (0.5 * 256.0 / 255.0)) / scale;
    float Cg = (col.g - (0.5 * 256.0 / 255.0)) / scale;

    vec3 col_rgb;
    col_rgb.r = Y + Co - Cg;
    col_rgb.g = Y + Cg;
    col_rgb.b = Y - Co - Cg;

    return col_rgb;
}

float lum(vec3 color) {
    return dot(vec3(0.212671, 0.715160, 0.715160), color);
}

// Ray Tracing Gems II, Listing 49-1
vec3 ReconstructViewPosition(vec2 uv, vec4 cam_frustum, float view_z, float is_ortho) {
    vec3 p;
    p.xy = uv.xy * cam_frustum.zw + cam_frustum.xy;

    p.xy *= view_z * (1.0 - abs(is_ortho)) + is_ortho;
    p.z = view_z;

    return p;
}

float PixelRadiusToWorld(float unproject, float is_ortho, float pixel_radius, float view_z) {
     return pixel_radius * unproject * mix(view_z, 1.0, abs(is_ortho));
}

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct LightItem {
    vec4 pos_and_radius;
    vec4 col_and_shadowreg_index;
    vec4 dir_and_spot;
    vec4 u_and_unused;
    vec4 v_and_unused;
};

#define LIGHTS_BUF_STRIDE 5

const int LIGHT_TYPE_POINT = 0;
const int LIGHT_TYPE_SPHERE = 1;
const int LIGHT_TYPE_RECT = 2;
const int LIGHT_TYPE_DISK = 3;
const int LIGHT_TYPE_LINE = 4;

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
    mat4 view_matrix, proj_matrix, view_proj_no_translation, prev_view_proj_no_translation;
    mat4 inv_view_matrix, inv_proj_matrix, inv_view_proj_no_translation, delta_matrix;
    ShadowMapRegion shadowmap_regions[REN_MAX_SHADOWMAPS_TOTAL];
    vec4 sun_dir, sun_col, taa_info, frustum_info;
    vec4 clip_info, cam_pos_and_gamma, prev_cam_pos;
    vec4 res_and_fres, transp_params_and_time;
    vec4 wind_scroll, wind_scroll_prev;
    ProbeItem probes[REN_MAX_PROBES_TOTAL];
    EllipsItem ellipsoids[REN_MAX_ELLIPSES_TOTAL];
};

struct MaterialData {
    uint texture_indices[REN_MAX_TEX_PER_MATERIAL];
    uint _pad[2];
    vec4 params[3];
};

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
}

#if defined(VULKAN) || defined(GL_SPIRV)
#define LAYOUT(x) layout(x)
#else
#define LAYOUT(x)
#endif

#line 0

#if defined(VULKAN)
#define GetCellIndex(ix, iy, slice, res) \
    (slice * REN_GRID_RES_X * REN_GRID_RES_Y + ((int(res.y) - 1 - iy) * REN_GRID_RES_Y / int(res.y)) * REN_GRID_RES_X + ix * REN_GRID_RES_X / int(res.x))
#else
#define GetCellIndex(ix, iy, slice, res) \
    (slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / int(res.y)) * REN_GRID_RES_X + ix * REN_GRID_RES_X / int(res.x))
#endif

// Rounds value to the nearest multiple of 8
uvec2 RoundUp8(uvec2 value) {
    uvec2 round_down = value & ~7u; // 0b111
    return (round_down == value) ? value : value + 8;
}

vec3 heatmap(float t) {
    vec3 r = vec3(t) * 2.1 - vec3(1.8, 1.14, 0.3);
    return vec3(1.0) - r * r;
}

highp float rand(highp vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float mad(float a, float b, float c) {
    return a * b + c;
}

float pow3(float x) {
    return (x * x) * x;
}

float pow5(float x) {
    return (x * x) * (x * x) * x;
}

float pow6(float x) {
    return (x * x) * (x * x) * (x * x);
}

vec3 FresnelSchlickRoughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow5(1.0 - cos_theta);
}

vec3 LinearToSRGB(vec3 linearRGB) {
    bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
    vec3 higher = 1.055 * pow(linearRGB, vec3(1.0/2.4)) - vec3(0.055);
    vec3 lower = linearRGB * vec3(12.92);

    return mix(higher, lower, cutoff);
}

vec3 SRGBToLinear(vec3 sRGB) {
    bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
    vec3 higher = pow((sRGB + vec3(0.055))/vec3(1.055), vec3(2.4));
    vec3 lower = sRGB/vec3(12.92);

    return mix(higher, lower, cutoff);
}

vec3 EvalSHIrradiance(vec3 normal, vec3 sh_l_00, vec3 sh_l_10, vec3 sh_l_11,
                      vec3 sh_l_12) {
    return max((0.5 + (sh_l_10 * normal.y + sh_l_11 * normal.z +
                       sh_l_12 * normal.x)) * sh_l_00 * 2.0, vec3(0.0));
}

vec3 EvalSHIrradiance_NonLinear(vec3 normal, vec3 sh_l_00, vec3 sh_l_10, vec3 sh_l_11,
                                vec3 sh_l_12) {
    vec3 l = sqrt(sh_l_10 * sh_l_10 + sh_l_11 * sh_l_11 + sh_l_12 * sh_l_12);
    vec3 inv_l = mix(vec3(0.0), vec3(1.0) / l, step(l, vec3(FLT_EPS)));

    vec3 q = 0.5 * (vec3(1.0) + (sh_l_10 * normal.y + sh_l_11 * normal.z +
                                 sh_l_12 * normal.x) * inv_l);
    vec3 p = vec3(1.0) + 2.0 * l;
    vec3 a = (vec3(1.0) - l) / (vec3(1.0) + l);

    return sh_l_00 * (a + (vec3(1.0) - a) * (p + vec3(1.0)) * pow(q, p));
}

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

vec3 EvaluateSH(in vec3 normal, in vec4 sh_coeffs[3]) {
    const float SH_A0 = 0.886226952; // PI / sqrt(4.0 * Pi)
    const float SH_A1 = 1.02332675;  // sqrt(PI / 3.0)

    vec4 vv = vec4(SH_A0, SH_A1 * normal.yzx);

    return vec3(dot(sh_coeffs[0], vv), dot(sh_coeffs[1], vv), dot(sh_coeffs[2], vv));
}

//  LANE TO 8x8 MAPPING
//  ===================
//  00 01 08 09 10 11 18 19
//  02 03 0a 0b 12 13 1a 1b
//  04 05 0c 0d 14 15 1c 1d
//  06 07 0e 0f 16 17 1e 1f
//  20 21 28 29 30 31 38 39
//  22 23 2a 2b 32 33 3a 3b
//  24 25 2c 2d 34 35 3c 3d
//  26 27 2e 2f 36 37 3e 3f
uvec2 RemapLane8x8(uint lane) {
    return uvec2(bitfieldInsert(bitfieldExtract(lane, 2, 3), lane, 0, 1),
                 bitfieldInsert(bitfieldExtract(lane, 3, 3), bitfieldExtract(lane, 1, 2), 0, 2));
}

uint ReverseBits4(uint x) {
    x = ((x & 0x5u) << 1u) | (( x & 0xAu) >> 1u);
    x = ((x & 0x3u) << 2u) | (( x & 0xCu) >> 2u);
    return x;
}

// https://en.wikipedia.org/wiki/Ordered_dithering
// RESULT: [0; 15]
uint Bayer4x4ui(uvec2 sample_pos, uint frame) {
    uvec2 sample_pos_wrap = sample_pos & 3;
    uint a = 2068378560u * (1u - (sample_pos_wrap.x >> 1u)) + 1500172770u * (sample_pos_wrap.x >> 1u);
    uint b = (sample_pos_wrap.y + ((sample_pos_wrap.x & 1u) << 2u)) << 2u;

    uint sampleOffset = frame;
#if 1 // BAYER_REVERSEBITS
    sampleOffset = ReverseBits4(sampleOffset);
#endif

    return ((a >> b) + sampleOffset) & 0xFu;
}

// RESULT: [0; 1)
float Bayer4x4(uvec2 sample_pos, uint frame) {
    uint bayer = Bayer4x4ui(sample_pos, frame);
    return float(bayer) / 16.0;
}

#endif // _CS_COMMON_GLSL

#line 15
#line 0
#ifndef GI_COMMON_GLSL
#define GI_COMMON_GLSL

float GetEdgeStoppingNormalWeight(vec3 normal_p, vec3 normal_q, float sigma) {
    return pow(clamp(dot(normal_p, normal_q), 0.0, 1.0), sigma);
}

vec2 GetGeometryWeightParams(float plane_dist_sensitivity, vec3 Xv, vec3 Nv, float scale) {
    const float MeterToUnitsMultiplier = 0.0;

    float a = scale * plane_dist_sensitivity / (Xv.z + MeterToUnitsMultiplier);
    float b = -dot(Nv, Xv) * a;

    return vec2(a, b);
}

// SmoothStep
// REQUIREMENT: a < b
#define _SmoothStep01( x ) ( x * x * ( 3.0 - 2.0 * x ) )

float SmoothStep01(float x) { return _SmoothStep01(saturate(x)); }
vec2 SmoothStep01(vec2 x) { return _SmoothStep01(saturate(x)); }
vec3 SmoothStep01(vec3 x) { return _SmoothStep01(saturate(x)); }
vec4 SmoothStep01(vec4 x) { return _SmoothStep01(saturate(x)); }

/* mediump */ float GetEdgeStoppingPlanarDistanceWeight(vec2 geometry_weight_params, vec3 center_normal_vs, vec3 neighbor_point_vs) {
    float d = dot(center_normal_vs, neighbor_point_vs);
    return SmoothStep01(1.0 - abs(d * geometry_weight_params.x + geometry_weight_params.y));
}

uint PackRay(uvec2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal) {
    uint ray_x_15bit = ray_coord.x & 32767u; // 0b111111111111111
    uint ray_y_14bit = ray_coord.y & 16383u; // 0b11111111111111
    uint copy_horizontal_1bit = copy_horizontal ? 1u : 0u;
    uint copy_vertical_1bit = copy_vertical ? 1u : 0u;
    uint copy_diagonal_1bit = copy_diagonal ? 1u : 0u;

    return (copy_diagonal_1bit << 31u) | (copy_vertical_1bit << 30u) | (copy_horizontal_1bit << 29u) | (ray_y_14bit << 15u) | (ray_x_15bit << 0u);
}

void UnpackRayCoords(uint packed_ray, out uvec2 ray_coord, out bool copy_horizontal, out bool copy_vertical, out bool copy_diagonal) {
    ray_coord.x = (packed_ray >> 0u) & 32767u; // 0b111111111111111
    ray_coord.y = (packed_ray >> 15u) & 16383u; // 0b11111111111111
    copy_horizontal = ((packed_ray >> 29u) & 1u) != 0u; // 0b1
    copy_vertical = ((packed_ray >> 30u) & 1u) != 0u; // 0b1
    copy_diagonal = ((packed_ray >> 31u) & 1u) != 0u; // 0b1
}

vec3 SampleCosineHemisphere(float u, float v) {
    float phi = 2.0 * M_PI * v;

    float cos_phi = cos(phi);
    float sin_phi = sin(phi);

    float dir = sqrt(u);
    float k = sqrt(1.0 - u);
    return vec3(dir * cos_phi, dir * sin_phi, k);
}

mat3 CreateTBN(vec3 N) {
    vec3 U;
    if (abs(N.z) > 0.0) {
        float k = sqrt(N.y * N.y + N.z * N.z);
        U.x = 0.0; U.y = -N.z / k; U.z = N.y / k;
    } else {
        float k = sqrt(N.x * N.x + N.y * N.y);
        U.x = N.y / k; U.y = -N.x / k; U.z = 0.0;
    }

    mat3 TBN;
    TBN[0] = U;
    TBN[1] = cross(N, U);
    TBN[2] = N;
    return transpose(TBN);
}

/* mediump */ float Luminance(/* mediump */ vec3 color) { return max(lum(color), 0.001); }

/* mediump */ float ComputeTemporalVariance(/* mediump */ vec3 history_radiance, /* mediump */ vec3 radiance) {
    /* mediump */ float history_luminance = Luminance(history_radiance);
    /* mediump */ float luminance = Luminance(radiance);
    /* mediump */ float diff = abs(history_luminance - luminance) / max(max(history_luminance, luminance), 0.5);
    return diff * diff;
}

vec2 RotateVector(vec4 rotator, vec2 v) { return v.x * rotator.xz + v.y * rotator.yw; }
vec4 CombineRotators(vec4 r1, vec4 r2 ) { return r1.xyxy * r2.xxzz + r1.zwzw * r2.yyww; }

vec4 GetBlurKernelRotation(uvec2 pixel_pos, vec4 base_rotator, uint frame) {
    vec4 rotator = vec4(1, 0, 0, 1);

#ifdef PER_PIXEL_KERNEL_ROTATION
    float angle = Bayer4x4(pixel_pos, frame) * 2.0 * M_PI;

    float ca = cos(angle);
    float sa = sin(angle);

    rotator = vec4(ca, sa, -sa, ca);
#endif

    rotator = CombineRotators(base_rotator, rotator);

    return rotator;
}

// samples = 8, min distance = 0.5, average samples on radius = 2
// third component is distance from center
const vec3 g_Poisson8[8] = {
    vec3(-0.4706069, -0.4427112, +0.6461146),
    vec3(-0.9057375, +0.3003471, +0.9542373),
    vec3(-0.3487388, +0.4037880, +0.5335386),
    vec3(+0.1023042, +0.6439373, +0.6520134),
    vec3(+0.5699277, +0.3513750, +0.6695386),
    vec3(+0.2939128, -0.1131226, +0.3149309),
    vec3(+0.7836658, -0.4208784, +0.8895339),
    vec3(+0.1564120, -0.8198990, +0.8346850)
};

// samples = 16, min distance = 0.38, average samples on radius = 2
const vec3 g_Poisson16[16] =
{
    vec3(-0.0936476, -0.7899283, +0.7954600),
    vec3(-0.1209752, -0.2627860, +0.2892948),
    vec3(-0.5646901, -0.7059856, +0.9040413),
    vec3(-0.8277994, -0.1538168, +0.8419688),
    vec3(-0.4620740, +0.1951437, +0.5015910),
    vec3(-0.7517998, +0.5998214, +0.9617633),
    vec3(-0.0812514, +0.2904110, +0.3015631),
    vec3(-0.2397440, +0.7581663, +0.7951688),
    vec3(+0.2446934, +0.9202285, +0.9522055),
    vec3(+0.4943011, +0.5736654, +0.7572486),
    vec3(+0.3415412, +0.1412707, +0.3696049),
    vec3(+0.8744238, +0.3246290, +0.9327384),
    vec3(+0.7406740, -0.1434729, +0.7544418),
    vec3(+0.3658852, -0.3596551, +0.5130534),
    vec3(+0.7880974, -0.5802425, +0.9786618),
    vec3(+0.3776688, -0.7620423, +0.8504953)
};

#endif // GI_COMMON_GLSL

#line 16
#line 0
#ifndef GI_CLASSIFY_TILES_INTERFACE_H
#define GI_CLASSIFY_TILES_INTERFACE_H

#line 0

#ifndef INTERFACE_COMMON_GLSL
#define INTERFACE_COMMON_GLSL

#ifdef __cplusplus
#define VEC2_TYPE Ren::Vec2f
#define VEC3_TYPE Ren::Vec3f
#define VEC4_TYPE Ren::Vec4f

#define IVEC2_TYPE Ren::Vec2i
#define IVEC3_TYPE Ren::Vec3i
#define IVEC4_TYPE Ren::Vec4i

#define UINT_TYPE uint32_t
#define UVEC2_TYPE Ren::Vec2u
#define UVEC3_TYPE Ren::Vec3u
#define UVEC4_TYPE Ren::Vec4u

#define MAT2_TYPE Ren::Mat2f
#define MAT3_TYPE Ren::Mat3f
#define MAT4_TYPE Ren::Mat4f

#define INTERFACE_START(name) namespace name {
#define INTERFACE_END }

#define DEF_CONST_INT(name, index) const int name = index;
#else // __cplusplus
#define VEC2_TYPE vec2
#define VEC3_TYPE vec3
#define VEC4_TYPE vec4

#define IVEC2_TYPE ivec2
#define IVEC3_TYPE ivec3
#define IVEC4_TYPE ivec4

#define UINT_TYPE uint
#define UVEC2_TYPE uvec2
#define UVEC3_TYPE uvec3
#define UVEC4_TYPE uvec4

#define MAT2_TYPE mat2
#define MAT3_TYPE mat3
#define MAT4_TYPE mat4

#define INTERFACE_START(name)
#define INTERFACE_END

#if defined(VULKAN)
#define LAYOUT_PARAMS layout(push_constant)
#elif defined(GL_SPIRV)
#define LAYOUT_PARAMS layout(binding = REN_UB_UNIF_PARAM_LOC, std140)
#else
#define LAYOUT_PARAMS layout(std140)
#endif
#endif // __cplusplus

#endif // INTERFACE_COMMON_GLSL

#line 0

INTERFACE_START(GIClassifyTiles)

struct Params {
    UVEC2_TYPE img_size;
    VEC2_TYPE thresholds;
    UVEC2_TYPE samples_and_guided;
    UINT_TYPE frame_index;
};

#define LOCAL_GROUP_SIZE_X 8
#define LOCAL_GROUP_SIZE_Y 8

#define DEPTH_TEX_SLOT 2
#define VARIANCE_TEX_SLOT 4
#define RAY_COUNTER_SLOT 5
#define RAY_LIST_SLOT 6
#define TILE_LIST_SLOT 7
#define SOBOL_BUF_SLOT 8
#define SCRAMLING_TILE_BUF_SLOT 9
#define RANKING_TILE_BUF_SLOT 10

#define GI_IMG_SLOT 0
#define NOISE_IMG_SLOT 1

INTERFACE_END

#endif // GI_CLASSIFY_TILES_INTERFACE_H

#line 17

/*
UNIFORM_BLOCKS
    UniformParams : 24
PERM @NO_SUBGROUP_EXTENSIONS
*/

#if !defined(NO_SUBGROUP_EXTENSIONS) && (!defined(GL_KHR_shader_subgroup_basic) || !defined(GL_KHR_shader_subgroup_ballot) || !defined(GL_KHR_shader_subgroup_shuffle) || !defined(GL_KHR_shader_subgroup_vote))
#define NO_SUBGROUP_EXTENSIONS
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_hist_tex;

layout(std430, binding = RAY_COUNTER_SLOT) coherent buffer RayCounter {
    uint g_ray_counter[];
};
layout(std430, binding = RAY_LIST_SLOT) writeonly buffer RayList {
    uint g_ray_list[];
};
layout(std430, binding = TILE_LIST_SLOT) writeonly buffer TileList {
    uint g_tile_list[];
};
layout(binding = SOBOL_BUF_SLOT) uniform highp usamplerBuffer g_sobol_seq_tex;
layout(binding = SCRAMLING_TILE_BUF_SLOT) uniform highp usamplerBuffer g_scrambling_tile_tex;
layout(binding = RANKING_TILE_BUF_SLOT) uniform highp usamplerBuffer g_ranking_tile_tex;

layout(binding = GI_IMG_SLOT, rgba16f) uniform writeonly image2D g_gi_img;
layout(binding = NOISE_IMG_SLOT, rgba8) uniform writeonly image2D g_noise_img;

bool IsDiffuseSurface(ivec2 px_coords) {
    // TODO: check metalness
    float depth = texelFetch(g_depth_tex, px_coords, 0).r;
    return depth < 1.0;
}

bool IsBaseRay(uvec2 dispatch_thread_id, uint samples_per_quad) {
    switch (samples_per_quad) {
    case 1:
        return ((dispatch_thread_id.x & 1u) | (dispatch_thread_id.y & 1u)) == 0u; // Deactivates 3 out of 4 rays
    case 2:
        return (dispatch_thread_id.x & 1u) == (dispatch_thread_id.y & 1u); // Deactivates 2 out of 4 rays. Keeps diagonal.
    default: // case 4:
        return true;
    }
}

uint GetBitMaskFromPixelPosition(uvec2 pixel_pos) {
    uint lane_index = (pixel_pos.y % 4) * 8 + (pixel_pos.x % 8);
    return (1u << lane_index);
}

void StoreRay(uint ray_index, uvec2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal) {
    g_ray_list[ray_index] = PackRay(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
}

shared uint g_tile_count;

// From https://github.com/GPUOpen-Effects/FidelityFX-Denoiser
/**********************************************************************
Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/
void ClassifyTiles(uvec2 dispatch_thread_id, uvec2 group_thread_id, uvec2 screen_size, uint samples_per_quad,
                   bool enable_temporal_variance_guided_tracing) {
    g_tile_count = 0;

#ifndef NO_SUBGROUP_EXTENSIONS
    bool is_first_lane_of_wave = subgroupElect();
#endif

    bool needs_ray = (dispatch_thread_id.x < screen_size.x && dispatch_thread_id.y < screen_size.y); // disable offscreen pixels

    // Dont shoot a ray on metal surfaces
    bool is_diffuse_surface = IsDiffuseSurface(ivec2(dispatch_thread_id));
    needs_ray = needs_ray && is_diffuse_surface;

    //
    bool needs_denoiser = needs_ray;

    // Decide which ray to keep
    bool is_base_ray = IsBaseRay(dispatch_thread_id, samples_per_quad);
    needs_ray = needs_ray && (!needs_denoiser || is_base_ray);

    if (enable_temporal_variance_guided_tracing && needs_denoiser && !needs_ray) {
        const float TemporalVarianceThreshold = 0.001;
        bool has_temporal_variance = texelFetch(g_variance_hist_tex, ivec2(dispatch_thread_id), 0).r > TemporalVarianceThreshold;
        needs_ray = needs_ray || has_temporal_variance;
    }

    groupMemoryBarrier(); // Wait until g_tile_count is cleared - allow some computations before and after
    barrier();

    // Now we know for each thread if it needs to shoot a ray and wether or not a denoiser pass has to run on this pixel.

    if (is_diffuse_surface) {
        atomicAdd(g_tile_count, 1u);
    }

    // Next we have to figure out which pixels that ray is creating the values for. Thus, if we have to copy its value horizontal, vertical or across.
    bool require_copy = !needs_ray && needs_denoiser; // Our pixel only requires a copy if we want to run a denoiser on it but don't want to shoot a ray for it.
#ifndef NO_SUBGROUP_EXTENSIONS
     // Subgroup reads need to be unconditional (should be first), probably a compiler bug!!!
    bool copy_horizontal = subgroupShuffleXor(require_copy, 1u) && (samples_per_quad != 4u) && is_base_ray; // 0b01 QuadReadAcrossX
    bool copy_vertical = subgroupShuffleXor(require_copy, 2u) && (samples_per_quad == 1u) && is_base_ray; // 0b10 QuadReadAcrossY
    bool copy_diagonal = subgroupShuffleXor(require_copy, 3u) && (samples_per_quad == 1u) && is_base_ray; // 0b11 QuadReadAcrossDiagonal

    // Thus, we need to compact the rays and append them all at once to the ray list.
    uvec4 needs_ray_ballot = subgroupBallot(needs_ray);
    uint local_ray_index_in_wave = subgroupBallotExclusiveBitCount(needs_ray_ballot);
    uint wave_ray_count = subgroupBallotBitCount(needs_ray_ballot);

    uint base_ray_index = 0; // leaving this uninitialized causes problems in opengl (???)
    if (is_first_lane_of_wave) {
        // increment ray counter
        base_ray_index = atomicAdd(g_ray_counter[0], wave_ray_count);
    }
    base_ray_index = subgroupBroadcastFirst(base_ray_index);
    if (needs_ray) {
        uint ray_index = base_ray_index + local_ray_index_in_wave;
        StoreRay(ray_index, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }
#else
    // TODO: Fallback using shared memory
    bool copy_horizontal = /*[group_thread_id.y][group_thread_id.x ^ 1u] &&*/ (samples_per_quad != 4u) && is_base_ray;
    bool copy_vertical = /*subgroupShuffleXor(require_copy, 2u) &&*/ (samples_per_quad == 1u) && is_base_ray;
    bool copy_diagonal = /*subgroupShuffleXor(require_copy, 3u) &&*/ (samples_per_quad == 1u) && is_base_ray;

    if (needs_ray) {
        uint ray_index = atomicAdd(g_ray_counter[0], 1);
        StoreRay(ray_index, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }
#endif

    // TODO: remove this
    imageStore(g_gi_img, ivec2(dispatch_thread_id), vec4(0.0));

    groupMemoryBarrier(); // Wait until all waves write into g_tile_count
    barrier();

    if (group_thread_id.x == 0u && group_thread_id.y == 0u && g_tile_count > 0) {
        uint tile_index = atomicAdd(g_ray_counter[2], 1);
        g_tile_list[tile_index] = ((dispatch_thread_id.y & 0xffffu) << 16u) | (dispatch_thread_id.x & 0xffffu);
    }
}

//
// https://eheitzresearch.wordpress.com/762-2/
//
float SampleRandomNumber(in uvec2 pixel, in uint sample_index, in uint sample_dimension) {
    // wrap arguments
    uint pixel_i = pixel.x & 127u;
    uint pixel_j = pixel.y & 127u;
    sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

    // xor index based on optimized ranking
    uint ranked_sample_index = sample_index ^ texelFetch(g_ranking_tile_tex, int((sample_dimension & 7u) + (pixel_i + pixel_j * 128u) * 8u)).r;

    // fetch value in sequence
    uint value = texelFetch(g_sobol_seq_tex, int(sample_dimension + ranked_sample_index * 256u)).r;

    // if the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ texelFetch(g_scrambling_tile_tex, int((sample_dimension & 7u) + (pixel_i + pixel_j * 128u) * 8u)).r;

    // convert to float and return
    return (float(value) + 0.5) / 256.0;
}

vec4 SampleRandomVector2D(uvec2 pixel) {
    return vec4(fract(SampleRandomNumber(pixel, 0, 2u) + float(g_params.frame_index & 0xFFu) * GOLDEN_RATIO),
                fract(SampleRandomNumber(pixel, 0, 3u) + float(g_params.frame_index & 0xFFu) * GOLDEN_RATIO),
                fract(SampleRandomNumber(pixel, 0, 4u) + float(g_params.frame_index & 0xFFu) * GOLDEN_RATIO),
                fract(SampleRandomNumber(pixel, 0, 5u) + float(g_params.frame_index & 0xFFu) * GOLDEN_RATIO));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x < 128u && gl_GlobalInvocationID.y < 128u) {
        imageStore(g_noise_img, ivec2(gl_GlobalInvocationID.xy), SampleRandomVector2D(gl_GlobalInvocationID.xy));
    }
    uvec2 group_id = gl_WorkGroupID.xy;
    uint group_index = gl_LocalInvocationIndex;
    uvec2 group_thread_id = RemapLane8x8(group_index);
    uvec2 dispatch_thread_id = group_id * 8u + group_thread_id;
    if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
        return;
    }

    ClassifyTiles(dispatch_thread_id, group_thread_id, g_params.img_size,
                  g_params.samples_and_guided.x, g_params.samples_and_guided.y != 0u);
}
