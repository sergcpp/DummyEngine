#version 430
#extension GL_ARB_shading_language_packing : require

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

#line 10
#line 0
#ifndef SSR_COMMON_GLSL
#define SSR_COMMON_GLSL

const float RoughnessSigmaMin = 0.001;
const float RoughnessSigmaMax = 0.01;

float GetEdgeStoppingNormalWeight(vec3 normal_p, vec3 normal_q, float sigma) {
    return pow(clamp(dot(normal_p, normal_q), 0.0, 1.0), sigma);
}

float GetEdgeStoppingRoughnessWeight(float roughness_p, float roughness_q, float sigma_min, float sigma_max) {
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
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

// http://jcgt.org/published/0007/04/01/paper.pdf
// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
vec3 SampleGGXVNDF(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2) {
    // Section 3.2: transforming the view direction to the hemisphere configuration
    vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
    vec3 T2 = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    float r = sqrt(U1);
    float phi = 2.0 * M_PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    vec3 Ne = normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    return Ne;
}

vec3 Sample_GGX_VNDF_Ellipsoid(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2) {
    return SampleGGXVNDF(Ve, alpha_x, alpha_y, U1, U2);
}

vec3 Sample_GGX_VNDF_Hemisphere(vec3 Ve, float alpha, float U1, float U2) {
    return Sample_GGX_VNDF_Ellipsoid(Ve, alpha, alpha, U1, U2);
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

/* mediump */ float Luminance(/* mediump */ vec3 color) { return max(dot(color, vec3(0.299, 0.587, 0.114)), 0.001); }

/* mediump */ float ComputeTemporalVariance(/* mediump */ vec3 history_radiance, /* mediump */ vec3 radiance) {
    /* mediump */ float history_luminance = Luminance(history_radiance);
    /* mediump */ float luminance = Luminance(radiance);
    /* mediump */ float diff = abs(history_luminance - luminance) / max(max(history_luminance, luminance), 0.5);
    return diff * diff;
}

#endif // SSR_COMMON_GLSL

#line 11
#line 0
#ifndef SSR_REPROJECT_INTERFACE_H
#define SSR_REPROJECT_INTERFACE_H

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

INTERFACE_START(SSRReproject)

struct Params {
    UVEC2_TYPE img_size;
    VEC2_TYPE thresholds;
};

#define LOCAL_GROUP_SIZE_X 8
#define LOCAL_GROUP_SIZE_Y 8

#define DEPTH_TEX_SLOT 4
#define NORM_TEX_SLOT 5
#define DEPTH_HIST_TEX_SLOT 6
#define NORM_HIST_TEX_SLOT 7
#define REFL_TEX_SLOT 8
#define RAYLEN_TEX_SLOT 9
#define REFL_HIST_TEX_SLOT 10
#define VELOCITY_TEX_SLOT 11
#define VARIANCE_HIST_TEX_SLOT 12
#define SAMPLE_COUNT_HIST_TEX_SLOT 13
#define TILE_LIST_BUF_SLOT 14

#define OUT_REPROJECTED_IMG_SLOT 0
#define OUT_AVG_REFL_IMG_SLOT 1
#define OUT_VERIANCE_IMG_SLOT 2
#define OUT_SAMPLE_COUNT_IMG_SLOT 3

INTERFACE_END

#endif // SSR_REPROJECT_INTERFACE_H

#line 12

/*
UNIFORM_BLOCKS
    SharedDataBlock : 23
    UniformParams : 24
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;
layout(binding = DEPTH_HIST_TEX_SLOT) uniform sampler2D g_depth_hist_tex;
layout(binding = NORM_HIST_TEX_SLOT) uniform sampler2D g_norm_hist_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = RAYLEN_TEX_SLOT) uniform sampler2D g_raylen_tex;
layout(binding = REFL_HIST_TEX_SLOT) uniform sampler2D g_refl_hist_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = VARIANCE_HIST_TEX_SLOT) uniform sampler2D g_variance_hist_tex;
layout(binding = SAMPLE_COUNT_HIST_TEX_SLOT) uniform sampler2D g_sample_count_hist_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_REPROJECTED_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_reprojected_img;
layout(binding = OUT_AVG_REFL_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_avg_refl_img;
layout(binding = OUT_VERIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;
layout(binding = OUT_SAMPLE_COUNT_IMG_SLOT, r16f) uniform image2D g_out_sample_count_img;

bool IsGlossyReflection(float roughness) {
    return roughness <= g_params.thresholds.x;
}

shared uint g_shared_storage_0[16][16];
shared uint g_shared_storage_1[16][16];

void LoadIntoSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage
    /* mediump */ vec3 refl[4];

    // Start from the upper left corner of 16x16 region
    dispatch_thread_id -= ivec2(4);

    // Load into registers
    for (int i = 0; i < 4; ++i) {
        refl[i] = texelFetch(g_refl_tex, dispatch_thread_id + offset[i], 0).xyz;
    }

    // Move to shared memory
    for (int i = 0; i < 4; ++i) {
        ivec2 index = group_thread_id + offset[i];
        g_shared_storage_0[index.y][index.x] = packHalf2x16(refl[i].xy);
        g_shared_storage_1[index.y][index.x] = packHalf2x16(refl[i].zz);
    }
}

/* mediump */ vec4 LoadFromGroupSharedMemoryRaw(ivec2 idx) {
    return vec4(unpackHalf2x16(g_shared_storage_0[idx.y][idx.x]),
                unpackHalf2x16(g_shared_storage_1[idx.y][idx.x]));
}

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

#define GAUSSIAN_K 3.0

#define LOCAL_NEIGHBORHOOD_RADIUS 4
#define REPROJECTION_NORMAL_SIMILARITY_THRESHOLD 0.9999
#define AVG_RADIANCE_LUMINANCE_WEIGHT 0.3
#define REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT 1.5
#define DISOCCLUSION_NORMAL_WEIGHT 1.4
#define DISOCCLUSION_DEPTH_WEIGHT 1.0
#define DISOCCLUSION_THRESHOLD 0.9
#define SAMPLES_FOR_ROUGHNESS(r) (1.0 - exp(-r * 100.0))

/* mediump */ float GetLuminanceWeight(/* mediump */ vec3 val) {
    /* mediump */ float luma = Luminance(val.xyz);
    /* mediump */ float weight = max(exp(-luma * AVG_RADIANCE_LUMINANCE_WEIGHT), 1.0e-2);
    return weight;
}

/* mediump */ float LocalNeighborhoodKernelWeight(/* mediump */ float i) {
    const /* mediump */ float radius = LOCAL_NEIGHBORHOOD_RADIUS + 1.0;
    return exp(-GAUSSIAN_K * (i * i) / (radius * radius));
}

struct moments_t {
    /* mediump */ vec3 mean;
    /* mediump */ vec3 variance;
};

moments_t EstimateLocalNeighbourhoodInGroup(ivec2 group_thread_id) {
    moments_t ret;
    ret.mean = vec3(0.0);
    ret.variance = vec3(0.0);

    /* mediump */ float accumulated_weight = 0;
    for (int j = -LOCAL_NEIGHBORHOOD_RADIUS; j <= LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (int i = -LOCAL_NEIGHBORHOOD_RADIUS; i <= LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            ivec2 index = group_thread_id + ivec2(i, j);
            /* mediump */ vec3 radiance = LoadFromGroupSharedMemoryRaw(index).rgb;
            /* mediump */ float weight = LocalNeighborhoodKernelWeight(i) * LocalNeighborhoodKernelWeight(j);
            accumulated_weight += weight;

            ret.mean += radiance * weight;
            ret.variance += radiance * radiance * weight;
        }
    }

    ret.mean /= accumulated_weight;
    ret.variance /= accumulated_weight;

    ret.variance = abs(ret.variance - ret.mean * ret.mean);

    return ret;
}

vec2 GetHitPositionReprojection(ivec2 dispatch_thread_id, vec2 uv, float reflected_ray_length) {
    float z = texelFetch(g_depth_tex, dispatch_thread_id, 0).r;
    vec3 ray_vs = vec3(uv, z);

    vec2 unjitter = g_shrd_data.taa_info.xy;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif

    { // project from screen space to view space
        ray_vs.y = (1.0 - ray_vs.y);
        ray_vs.xy = 2.0 * ray_vs.xy - 1.0;
        ray_vs.xy -= unjitter;
        vec4 projected = g_shrd_data.inv_proj_matrix * vec4(ray_vs, 1.0);
        ray_vs = projected.xyz / projected.w;
    }

    // We start out with reconstructing the ray length in view space.
    // This includes the portion from the camera to the reflecting surface as well as the portion from the surface to the hit position.
    float surface_depth = length(ray_vs);
    float ray_length = surface_depth + reflected_ray_length;

    // We then perform a parallax correction by shooting a ray of the same length "straight through" the reflecting surface and reprojecting the tip of that ray to the previous frame.
    ray_vs /= surface_depth; // == normalize(ray_vs)
    ray_vs *= ray_length;
    vec3 hit_position_ws; // This is the "fake" hit position if we would follow the ray straight through the surface.
    { // project from view space to world space
        // TODO: use matrix without translation here
        vec4 projected = g_shrd_data.inv_view_matrix * vec4(ray_vs, 1.0);
        hit_position_ws = projected.xyz;
    }

    vec2 prev_hit_position;
    { // project to screen space of previous frame
        vec4 projected = g_shrd_data.prev_view_proj_no_translation * vec4(hit_position_ws - g_shrd_data.prev_cam_pos.xyz, 1.0);
        projected.xyz /= projected.w;
        projected.xy = 0.5 * projected.xy + 0.5;
        projected.y = (1.0 - projected.y);
        prev_hit_position = projected.xy;
    }
    return prev_hit_position;
}

/* mediump */ float GetDisocclusionFactor(/* mediump */ vec3 normal, /* mediump */ vec3 history_normal, float linear_depth, float history_linear_depth) {
    /* mediump */ float factor = 1.0
                        * exp(-abs(1.0 - max(0.0, dot(normal, history_normal))) * DISOCCLUSION_NORMAL_WEIGHT)
                        * exp(-abs(history_linear_depth - linear_depth) / linear_depth * DISOCCLUSION_DEPTH_WEIGHT);
    return factor;
}

void PickReprojection(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size,
                      /* mediump */ float roughness, /* mediump */ float ray_len,
                      /* mediump */ out float disocclusion_factor, out vec2 reprojection_uv,
                      /* mediump */ out vec3 reprojection) {
    moments_t local_neighborhood = EstimateLocalNeighbourhoodInGroup(group_thread_id);

    vec2 uv = (vec2(dispatch_thread_id) + vec2(0.5)) / vec2(screen_size);
    /* mediump */ vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0)).xyz;
    /* mediump */ vec3 history_normal;
    float history_linear_depth;

    {
        vec2 motion_vector = texelFetch(g_velocity_tex, ivec2(dispatch_thread_id), 0).xy;
        vec2 surf_repr_uv = uv - motion_vector;
        vec2 hit_repr_uv = GetHitPositionReprojection(ivec2(dispatch_thread_id), uv, ray_len);

        /* mediump */ vec3 surf_history = textureLod(g_refl_hist_tex, surf_repr_uv, 0.0).xyz;
        /* mediump */ vec3 hit_history = textureLod(g_refl_hist_tex, hit_repr_uv, 0.0).xyz;

        /* mediump */ vec4 surf_fetch = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, surf_repr_uv, 0.0));
        /* mediump */ vec4 hit_fetch = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, hit_repr_uv, 0.0));

        /* mediump */ vec3 surf_normal = surf_fetch.xyz, hit_normal = hit_fetch.xyz;
        /* mediump */ float surf_roughness = surf_fetch.w, hit_roughness = hit_fetch.w;

        float hit_normal_similarity = dot(hit_normal, normal);
        float surf_normal_similarity = dot(surf_normal, normal);

        // Choose reprojection uv based on similarity to the local neighborhood
        if (hit_normal_similarity > REPROJECTION_NORMAL_SIMILARITY_THRESHOLD &&
            hit_normal_similarity + 1.0e-3 > surf_normal_similarity &&
            abs(hit_roughness - roughness) < abs(surf_roughness - roughness) + 1.0e-3) {
            // Mirror reflection
            history_normal = hit_normal;
            float hit_history_depth = textureLod(g_depth_hist_tex, hit_repr_uv, 0.0).x;
            history_linear_depth = LinearizeDepth(hit_history_depth, g_shrd_data.clip_info);
            reprojection_uv = hit_repr_uv;
            reprojection = hit_history;
        } else {
            // Reject surface reprojection based on simple distance
            if (length2(surf_history - local_neighborhood.mean) <
                REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT * length(local_neighborhood.variance)) {
                // Surface reflection
                history_normal = surf_normal;
                float surf_history_depth = textureLod(g_depth_hist_tex, surf_repr_uv, 0.0).x;
                history_linear_depth = LinearizeDepth(surf_history_depth, g_shrd_data.clip_info);
                reprojection_uv = surf_repr_uv;
                reprojection = surf_history;
            } else {
                disocclusion_factor = 0.0;
                return;
            }
        }

        float depth = texelFetch(g_depth_tex, ivec2(dispatch_thread_id), 0).x;
        float linear_depth  = LinearizeDepth(depth, g_shrd_data.clip_info);
        // Determine disocclusion factor based on history
        disocclusion_factor = GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);

        if (disocclusion_factor > DISOCCLUSION_THRESHOLD) {
            return;
        }

        // Try to find the closest sample in the vicinity if we are not convinced of a disocclusion
        if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
            vec2 closest_uv = reprojection_uv;
            vec2 dudv = 1.0 / vec2(screen_size);

            const int SearchRadius = 1;
            for (int y = -SearchRadius; y <= SearchRadius; ++y) {
                for (int x = -SearchRadius; x <= SearchRadius; ++x) {
                    vec2 uv = reprojection_uv + vec2(x, y) * dudv;
                    /* mediump */ vec3 history_normal = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, uv, 0.0)).xyz;
                    float history_depth = textureLod(g_depth_hist_tex, uv, 0.0).x;
                    float history_linear_depth = LinearizeDepth(history_depth, g_shrd_data.clip_info);
                    /* mediump */ float weight = GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
                    if (weight > disocclusion_factor) {
                        disocclusion_factor = weight;
                        closest_uv = uv;
                        reprojection_uv = closest_uv;
                    }
                }
            }
            reprojection = textureLod(g_refl_hist_tex, reprojection_uv, 0.0).rgb;
        }

        // Try to get rid of potential leaks at bilinear interpolation level
        if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
            // If we've got a discarded history, try to construct a better sample out of 2x2 interpolation neighborhood
            // Helps quite a bit on the edges in movement
            float uvx = fract(float(screen_size.x) * reprojection_uv.x + 0.5);
            float uvy = fract(float(screen_size.y) * reprojection_uv.y + 0.5);
            ivec2 reproject_texel_coords = ivec2(vec2(screen_size) * reprojection_uv - vec2(0.5));

            /* mediump */ vec3 reprojection00 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(0, 0), 0).rgb;
            /* mediump */ vec3 reprojection10 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(1, 0), 0).rgb;
            /* mediump */ vec3 reprojection01 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(0, 1), 0).rgb;
            /* mediump */ vec3 reprojection11 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(1, 1), 0).rgb;

            /* mediump */ vec3 normal00 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(0, 0), 0)).xyz;
            /* mediump */ vec3 normal10 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(1, 0), 0)).xyz;
            /* mediump */ vec3 normal01 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(0, 1), 0)).xyz;
            /* mediump */ vec3 normal11 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(1, 1), 0)).xyz;

            float depth00 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(0, 0), 0).x, g_shrd_data.clip_info);
            float depth10 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(1, 0), 0).x, g_shrd_data.clip_info);
            float depth01 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(0, 1), 0).x, g_shrd_data.clip_info);
            float depth11 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(1, 1), 0).x, g_shrd_data.clip_info);

            /* mediump */ vec4 w;
            // Initialize with occlusion weights
            w.x = GetDisocclusionFactor(normal, normal00, linear_depth, depth00) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
            w.y = GetDisocclusionFactor(normal, normal10, linear_depth, depth10) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
            w.z = GetDisocclusionFactor(normal, normal01, linear_depth, depth01) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
            w.w = GetDisocclusionFactor(normal, normal11, linear_depth, depth11) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
            // And then mix in bilinear weights
            w.x = w.x * (1.0 - uvx) * (1.0 - uvy);
            w.y = w.y * (uvx) * (1.0 - uvy);
            w.z = w.z * (1.0 - uvx) * (uvy);
            w.w = w.w * (uvx) * (uvy);
            /* mediump */ float ws = max(w.x + w.y + w.z + w.w, 1.0e-3);
            // normalize
            w /= ws;

            /* mediump */ vec3 history_normal;
            float history_linear_depth;
            reprojection = reprojection00 * w.x + reprojection10 * w.y + reprojection01 * w.z + reprojection11 * w.w;
            history_linear_depth = depth00 * w.x + depth10 * w.y + depth01 * w.z + depth11 * w.w;
            history_normal = normal00 * w.x + normal10 * w.y + normal01 * w.z + normal11 * w.w;
            disocclusion_factor = GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
        }
        disocclusion_factor = disocclusion_factor < DISOCCLUSION_THRESHOLD ? 0.0 : disocclusion_factor;
    }
}

void Reproject(uvec2 dispatch_thread_id, uvec2 group_thread_id, uvec2 screen_size, float temporal_stability_factor, int max_samples) {
    LoadIntoSharedMemory(ivec2(dispatch_thread_id), ivec2(group_thread_id), ivec2(screen_size));

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // center threads in shared memory

    /* mediump */ float variance = 1.0;
    /* mediump */ float sample_count = 0.0;
    /* mediump */ float roughness;
    vec3 normal;
    vec4 fetch = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0));
    normal = fetch.xyz;
    roughness = fetch.w;
    /* mediump */ vec3 refl = texelFetch(g_refl_tex, ivec2(dispatch_thread_id), 0).xyz;
    /* mediump */ float ray_len = texelFetch(g_raylen_tex, ivec2(dispatch_thread_id), 0).x;

    if (IsGlossyReflection(roughness)) {
        /* mediump */ float disocclusion_factor;
        vec2 reprojection_uv;
        /* mediump */ vec3 reprojection;
        PickReprojection(ivec2(dispatch_thread_id), ivec2(group_thread_id), screen_size, roughness, ray_len,
                         disocclusion_factor, reprojection_uv, reprojection);

        if (all(greaterThan(reprojection_uv, vec2(0.0))) && all(lessThan(reprojection_uv, vec2(1.0)))) {
            /* mediump */ float prev_variance = textureLod(g_variance_hist_tex, reprojection_uv, 0.0).x;
            sample_count = textureLod(g_sample_count_hist_tex, reprojection_uv, 0.0).x * disocclusion_factor;
            /* mediump */ float s_max_samples = max(8.0, max_samples * SAMPLES_FOR_ROUGHNESS(roughness));
            sample_count = min(s_max_samples, sample_count + 1);
            /* mediump */ float new_variance = ComputeTemporalVariance(refl, reprojection);
            if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0, 0.0, 0.0, 0.0));
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0));
            } else {
                /* mediump */ float variance_mix = mix(new_variance, prev_variance, 1.0 / sample_count);
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(reprojection, 0.0));
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(variance_mix));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(sample_count));
                // Mix in reprojection for radiance mip computation
                refl = mix(refl, reprojection, 0.3);
            }
        } else {
            imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
            imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
            imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0));
        }
    }

    // Downsample 8x8 -> 1 radiance using shared memory
    // Initialize shared array for downsampling
    /* mediump */ float weight = GetLuminanceWeight(refl);
    refl *= weight;
    if (any(greaterThanEqual(dispatch_thread_id, screen_size)) || any(isinf(refl)) || any(isnan(refl)) || weight > 1.0e3) {
        refl = vec3(0.0);
        weight = 0.0;
    }

    group_thread_id -= 4;

    g_shared_storage_0[group_thread_id.y][group_thread_id.x] = packHalf2x16(refl.xy);
    g_shared_storage_1[group_thread_id.y][group_thread_id.x] = packHalf2x16(vec2(refl.z, weight));

    groupMemoryBarrier();
    barrier();

    for (int i = 2; i <= 8; i = i * 2) {
        int ox = int(group_thread_id.x) * i;
        int oy = int(group_thread_id.y) * i;
        int ix = int(group_thread_id.x) * i + i / 2;
        int iy = int(group_thread_id.y) * i + i / 2;
        if (ix < 8 && iy < 8) {
            /* mediump */ vec4 rad_weight00 = LoadFromGroupSharedMemoryRaw(ivec2(ox, oy));
            /* mediump */ vec4 rad_weight10 = LoadFromGroupSharedMemoryRaw(ivec2(ox, iy));
            /* mediump */ vec4 rad_weight01 = LoadFromGroupSharedMemoryRaw(ivec2(ix, oy));
            /* mediump */ vec4 rad_weight11 = LoadFromGroupSharedMemoryRaw(ivec2(ix, iy));
            /* mediump */ vec4 sum = rad_weight00 + rad_weight01 + rad_weight10 + rad_weight11;

            g_shared_storage_0[group_thread_id.y][group_thread_id.x] = packHalf2x16(sum.xy);
            g_shared_storage_1[group_thread_id.y][group_thread_id.x] = packHalf2x16(sum.zw);
        }
        groupMemoryBarrier();
        barrier();
    }

    if (all(equal(group_thread_id, uvec2(0)))) {
        /* mediump */ vec4 sum = LoadFromGroupSharedMemoryRaw(ivec2(0));
        /* mediump */ float weight_acc = max(sum.w, 1.0e-3);
        vec3 radiance_avg = sum.xyz / weight_acc;

        imageStore(g_out_avg_refl_img, ivec2(dispatch_thread_id / 8), vec4(radiance_avg, 0.0));
    }
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    ivec2  dispatch_group_id = dispatch_thread_id / 8;
    uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    Reproject(remapped_dispatch_thread_id, remapped_group_thread_id, g_params.img_size, 0.7 /* temporal_stability */, 64);
}
