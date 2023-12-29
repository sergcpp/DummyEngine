#version 430
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_quad : enable
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

#line 12
#line 0
#ifndef DEPTH_HIERARCHY_INTERFACE_H
#define DEPTH_HIERARCHY_INTERFACE_H

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

INTERFACE_START(DepthHierarchy)

struct Params {
    IVEC4_TYPE depth_size;
    VEC4_TYPE clip_info;
};

#define LOCAL_GROUP_SIZE_X 64
#define LOCAL_GROUP_SIZE_Y 64

#define DEPTH_TEX_SLOT 14
#define ATOMIC_CNT_SLOT 15

#define DEPTH_IMG_SLOT 0

INTERFACE_END

#endif // DEPTH_HIERARCHY_INTERFACE_H

#line 13

/*
UNIFORM_BLOCKS
    UniformParams : 24
PERM @MIPS_7
PERM @NO_SUBGROUP_EXTENSIONS
PERM @MIPS_7@NO_SUBGROUP_EXTENSIONS
*/

#if !defined(NO_SUBGROUP_EXTENSIONS) && !defined(GL_KHR_shader_subgroup_quad)
#define NO_SUBGROUP_EXTENSIONS
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_tex;
layout(std430, binding = ATOMIC_CNT_SLOT) buffer AtomicCounter {
    uint g_atomic_counter;
};

#ifdef MIPS_7 // simplified version
layout(binding = DEPTH_IMG_SLOT, r32f) uniform image2D g_depth_hierarchy[7];
#else
layout(binding = DEPTH_IMG_SLOT, r32f) uniform image2D g_depth_hierarchy[13];
#endif

layout(local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

ivec2 limit_coords(ivec2 icoord) {
    return clamp(icoord, ivec2(0), g_params.depth_size.xy - 1);
}

#define REDUCE_OP min

float ReduceSrcDepth4(ivec2 base) {
    float v0 = texelFetch(g_depth_tex, limit_coords(base + ivec2(0, 0)), 0).r;
    float v1 = texelFetch(g_depth_tex, limit_coords(base + ivec2(0, 1)), 0).r;
    float v2 = texelFetch(g_depth_tex, limit_coords(base + ivec2(1, 0)), 0).r;
    float v3 = texelFetch(g_depth_tex, limit_coords(base + ivec2(1, 1)), 0).r;
    return REDUCE_OP(REDUCE_OP(v0, v1), REDUCE_OP(v2, v3));
}

#if !defined(NO_SUBGROUP_EXTENSIONS)
float ReduceQuad(float v) {
    float v0 = v;
    float v1 = subgroupQuadSwapHorizontal(v);
    float v2 = subgroupQuadSwapVertical(v);
    float v3 = subgroupQuadSwapDiagonal(v);
    return REDUCE_OP(REDUCE_OP(v0, v1), REDUCE_OP(v2, v3));
}
#endif

void WriteDstDepth(int index, ivec2 icoord, float v) {
    imageStore(g_depth_hierarchy[index], icoord, vec4(v));
}

shared float g_shared_depth[16][16];
shared uint g_shared_counter;

float ReduceIntermediate(ivec2 i0, ivec2 i1, ivec2 i2, ivec2 i3) {
    float v0 = g_shared_depth[i0.x][i0.y];
    float v1 = g_shared_depth[i1.x][i1.y];
    float v2 = g_shared_depth[i2.x][i2.y];
    float v3 = g_shared_depth[i3.x][i3.y];
    return REDUCE_OP(REDUCE_OP(v0, v1), REDUCE_OP(v2, v3));
}

float ReduceLoad4(ivec2 base) {
    float v0 = imageLoad(g_depth_hierarchy[6], base + ivec2(0, 0)).r;
    float v1 = imageLoad(g_depth_hierarchy[6], base + ivec2(0, 1)).r;
    float v2 = imageLoad(g_depth_hierarchy[6], base + ivec2(1, 0)).r;
    float v3 = imageLoad(g_depth_hierarchy[6], base + ivec2(1, 1)).r;
    return REDUCE_OP(REDUCE_OP(v0, v1), REDUCE_OP(v2, v3));
}

void DownsampleNext4Levels(int base_level, int levels_total, uvec2 work_group_id, uint x, uint y) {
    if (levels_total <= base_level + 1) return;
    { // Init mip level 3 or
#if defined(NO_SUBGROUP_EXTENSIONS)
        if (gl_LocalInvocationIndex < 64) {
            float v = ReduceIntermediate(ivec2(x * 2 + 0, y * 2 + 0), ivec2(x * 2 + 1, y * 2 + 0),
                                         ivec2(x * 2 + 0, y * 2 + 1), ivec2(x * 2 + 1, y * 2 + 1));
            WriteDstDepth(base_level + 1, ivec2(work_group_id * 8) + ivec2(x, y), v);
            // store to LDS, try to reduce bank conflicts
            // x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0 x
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
            // ...
            // x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
            g_shared_depth[x * 2 + y % 2][y * 2] = v;
        }
#else
        float v = ReduceQuad(g_shared_depth[x][y]);
        // quad index 0 stores result
        if ((gl_LocalInvocationIndex % 4) == 0) {
            WriteDstDepth(base_level + 1, ivec2(work_group_id * 8) + ivec2(x / 2, y / 2), v);
            g_shared_depth[x + (y / 2) % 2][y] = v;
        }
#endif
        barrier();
    }
    if (levels_total <= base_level + 2) return;
    { // Init mip level 4
#if defined(NO_SUBGROUP_EXTENSIONS)
        if (gl_LocalInvocationIndex < 16) {
            // x 0 x 0
            // 0 0 0 0
            // 0 x 0 x
            // 0 0 0 0
            float v = ReduceIntermediate(ivec2(x * 4 + 0 + 0, y * 4 + 0),
                                         ivec2(x * 4 + 2 + 0, y * 4 + 0),
                                         ivec2(x * 4 + 0 + 1, y * 4 + 2),
                                         ivec2(x * 4 + 2 + 1, y * 4 + 2));
            WriteDstDepth(base_level + 2, ivec2(work_group_id * 4) + ivec2(x, y), v);
            // store to LDS
            // x 0 0 0 x 0 0 0 x 0 0 0 x 0 0 0
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // 0 x 0 0 0 x 0 0 0 x 0 0 0 x 0 0
            // ...
            // 0 0 x 0 0 0 x 0 0 0 x 0 0 0 x 0
            // ...
            // 0 0 0 x 0 0 0 x 0 0 0 x 0 0 0 x
            // ...
            g_shared_depth[x * 4 + y][y * 4] = v;
        }
#else
        if (gl_LocalInvocationIndex < 64) {
            float v = ReduceQuad(g_shared_depth[x * 2 + y % 2][y * 2]);
            // quad index 0 stores result
            if ((gl_LocalInvocationIndex % 4) == 0) {
                WriteDstDepth(base_level + 2, ivec2(work_group_id * 4) + ivec2(x / 2, y / 2), v);
                g_shared_depth[x * 2 + y / 2][y * 2] = v;
            }
        }
#endif
        barrier();
    }
    if (levels_total <= base_level + 3) return;
    { // Init mip level 5
#if defined(NO_SUBGROUP_EXTENSIONS)
        if (gl_LocalInvocationIndex < 4) {
            // x 0 0 0 x 0 0 0
            // ...
            // 0 x 0 0 0 x 0 0
            float v = ReduceIntermediate(ivec2(x * 8 + 0 + 0 + y * 2, y * 8 + 0),
                                         ivec2(x * 8 + 4 + 0 + y * 2, y * 8 + 0),
                                         ivec2(x * 8 + 0 + 1 + y * 2, y * 8 + 4),
                                         ivec2(x * 8 + 4 + 1 + y * 2, y * 8 + 4));
            WriteDstDepth(base_level + 3, ivec2(work_group_id * 2) + ivec2(x, y), v);
            // store to LDS
            // x x x x 0 ...
            // 0 ...
            g_shared_depth[x + y * 2][0] = v;
        }
#else
        if (gl_LocalInvocationIndex < 16) {
            float v = ReduceQuad(g_shared_depth[x * 4 + y][y * 4]);
            // quad index 0 stores result
            if ((gl_LocalInvocationIndex % 4) == 0) {
                WriteDstDepth(base_level + 3, ivec2(work_group_id * 2) + ivec2(x / 2, y / 2), v);
                g_shared_depth[x / 2 + y][0] = v;
            }
        }
#endif
        barrier();
    }
    if (levels_total <= base_level + 4) return;
    { // Init mip level 6
#if defined(NO_SUBGROUP_EXTENSIONS)
        if (gl_LocalInvocationIndex < 1) {
            // x x x x 0 ...
            // 0 ...
            float v = ReduceIntermediate(ivec2(0, 0), ivec2(1, 0), ivec2(2, 0), ivec2(3, 0));
            WriteDstDepth(base_level + 4, ivec2(work_group_id), v);
        }
#else
        if (gl_LocalInvocationIndex < 4) {
            float v = ReduceQuad(g_shared_depth[gl_LocalInvocationIndex][0]);
            // quad index 0 stores result
            if ((gl_LocalInvocationIndex % 4) == 0) {
                WriteDstDepth(base_level + 4, ivec2(work_group_id), v);
            }
        }
#endif
        barrier();
    }
}

void main() {
    //
    // Taken from https://github.com/GPUOpen-Effects/FidelityFX-SPD
    //

    // Copy the first level
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 8; ++j) {
            ivec2 icoord = ivec2(2 * gl_GlobalInvocationID.x + i, 8 * gl_GlobalInvocationID.y + j);
            float depth_val = 0.0;
            if (icoord.x < g_params.depth_size.x && icoord.y < g_params.depth_size.y) {
                depth_val = texelFetch(g_depth_tex, icoord, 0).r;
            }
            imageStore(g_depth_hierarchy[0], icoord, vec4(depth_val));
        }
    }

    int required_mips = g_params.depth_size.z;
    if (required_mips <= 1) return;

    //
    // Remap index for easier reduction (rearrange to nested quads)
    //
    //  00 01 02 03 04 05 06 07           00 01 08 09 10 11 18 19
    //  08 09 0a 0b 0c 0d 0e 0f           02 03 0a 0b 12 13 1a 1b
    //  10 11 12 13 14 15 16 17           04 05 0c 0d 14 15 1c 1d
    //  18 19 1a 1b 1c 1d 1e 1f   ---->   06 07 0e 0f 16 17 1e 1f
    //  20 21 22 23 24 25 26 27           20 21 28 29 30 31 38 39
    //  28 29 2a 2b 2c 2d 2e 2f           22 23 2a 2b 32 33 3a 3b
    //  30 31 32 33 34 35 36 37           24 25 2c 2d 34 35 3c 3d
    //  38 39 3a 3b 3c 3d 3e 3f           26 27 2e 2f 36 37 3e 3f

    uint sub_64 = uint(gl_LocalInvocationIndex % 64);
    uvec2 sub_8x8 = uvec2(bitfieldInsert(bitfieldExtract(sub_64, 2, 3), sub_64, 0, 1),
                          bitfieldInsert(bitfieldExtract(sub_64, 3, 3), bitfieldExtract(sub_64, 1, 2), 0, 2));
    uint x = sub_8x8.x + 8 * ((gl_LocalInvocationIndex / 64) % 2);
    uint y = sub_8x8.y + 8 * ((gl_LocalInvocationIndex / 64) / 2);

    { // Init mip levels 1 and 2
        float v[4];
        ivec2 icoord;

        icoord = ivec2(gl_WorkGroupID.xy * 32) + ivec2(x, y);
        v[0] = ReduceSrcDepth4(icoord * 2);
        WriteDstDepth(1, icoord, v[0]);

        icoord = ivec2(gl_WorkGroupID.xy * 32) + ivec2(x + 16, y);
        v[1] = ReduceSrcDepth4(icoord * 2);
        WriteDstDepth(1, icoord, v[1]);

        icoord = ivec2(gl_WorkGroupID.xy * 32) + ivec2(x, y + 16);
        v[2] = ReduceSrcDepth4(icoord * 2);
        WriteDstDepth(1, icoord, v[2]);

        icoord = ivec2(gl_WorkGroupID.xy * 32) + ivec2(x + 16, y + 16);
        v[3] = ReduceSrcDepth4(icoord * 2);
        WriteDstDepth(1, icoord, v[3]);

        if (required_mips <= 2) return;

#if defined(NO_SUBGROUP_EXTENSIONS)
        for (int i = 0; i < 4; ++i) {
            g_shared_depth[x][y] = v[i];
            barrier();
            if (gl_LocalInvocationIndex < 64) {
                v[i] = ReduceIntermediate(ivec2(x * 2 + 0, y * 2 + 0), ivec2(x * 2 + 1, y * 2 + 0),
                                          ivec2(x * 2 + 0, y * 2 + 1), ivec2(x * 2 + 1, y * 2 + 1));
                WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x + (i % 2) * 8, y + (i / 2) * 8), v[i]);
            }
            barrier();
        }

        if (gl_LocalInvocationIndex < 64) {
            g_shared_depth[x + 0][y + 0] = v[0];
            g_shared_depth[x + 8][y + 0] = v[1];
            g_shared_depth[x + 0][y + 8] = v[2];
            g_shared_depth[x + 8][y + 8] = v[3];
        }
#else
        v[0] = ReduceQuad(v[0]);
        v[1] = ReduceQuad(v[1]);
        v[2] = ReduceQuad(v[2]);
        v[3] = ReduceQuad(v[3]);

        if ((gl_LocalInvocationIndex % 4) == 0) {
            WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x / 2, y / 2), v[0]);
            g_shared_depth[x / 2][y / 2] = v[0];

            WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x / 2 + 8, y / 2), v[1]);
            g_shared_depth[x / 2 + 8][y / 2] = v[1];

            WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x / 2, y / 2 + 8), v[2]);
            g_shared_depth[x / 2][y / 2 + 8] = v[2];

            WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x / 2 + 8, y / 2 + 8), v[3]);
            g_shared_depth[x / 2 + 8][y / 2 + 8] = v[3];
        }
#endif
        barrier();
    }

    DownsampleNext4Levels(2, required_mips, gl_WorkGroupID.xy, x, y);

#ifndef MIPS_7
    if (required_mips <= 7) return;

    // Only the last active workgroup should proceed
    if (gl_LocalInvocationIndex == 0) {
        g_shared_counter = atomicAdd(g_atomic_counter, 1);
    }
    barrier();
    if (g_shared_counter != (g_params.depth_size.w - 1)) {
        return;
    }
    g_atomic_counter = 0;

    { // Init mip levels 7 and 8
        float v[4];
        ivec2 icoord;

        icoord = ivec2(x * 2 + 0, y * 2 + 0);
        v[0] = ReduceLoad4(icoord * 2);
        WriteDstDepth(7, icoord, v[0]);

        icoord = ivec2(x * 2 + 1, y * 2 + 0);
        v[1] = ReduceLoad4(icoord * 2);
        WriteDstDepth(7, icoord, v[1]);

        icoord = ivec2(x * 2 + 0, y * 2 + 1);
        v[2] = ReduceLoad4(icoord * 2);
        WriteDstDepth(7, icoord, v[2]);

        icoord = ivec2(x * 2 + 1, y * 2 + 1);
        v[3] = ReduceLoad4(icoord * 2);
        WriteDstDepth(7, icoord, v[3]);

        if (required_mips <= 8) return;

        float vv = REDUCE_OP(REDUCE_OP(v[0], v[1]), REDUCE_OP(v[2], v[3]));
        WriteDstDepth(8, ivec2(x, y), vv);
        g_shared_depth[x][y] = vv;
        barrier();
    }

    DownsampleNext4Levels(8, required_mips, uvec2(0, 0), x, y);
#endif
}
