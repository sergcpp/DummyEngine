#version 430
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_shuffle : enable
#extension GL_KHR_shader_subgroup_vote : enable
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

#line 17
#line 0
#ifndef RT_SHADOW_CLASSIFY_TILES_INTERFACE_H
#define RT_SHADOW_CLASSIFY_TILES_INTERFACE_H

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

INTERFACE_START(RTShadowClassifyTiles)

struct Params {
    UVEC2_TYPE img_size;
    VEC2_TYPE inv_img_size;
};

#define LOCAL_GROUP_SIZE_X 8
#define LOCAL_GROUP_SIZE_Y 8

#define DEPTH_TEX_SLOT 2
#define VELOCITY_TEX_SLOT 3
#define NORM_TEX_SLOT 4
#define HISTORY_TEX_SLOT 5
#define PREV_DEPTH_TEX_SLOT 6
#define PREV_MOMENTS_TEX_SLOT 7
#define RAY_HITS_BUF_SLOT 8
#define OUT_TILE_METADATA_BUF_SLOT 9

#define OUT_REPROJ_RESULTS_IMG_SLOT 0
#define OUT_MOMENTS_IMG_SLOT 1

INTERFACE_END

#endif // RT_SHADOW_CLASSIFY_TILES_INTERFACE_H

#line 18
#line 0

#define TILE_SIZE_X 8
#define TILE_SIZE_Y 4

#define TILE_META_DATA_CLEAR_MASK 1u // 0b01u
#define TILE_META_DATA_LIGHT_MASK 2u // 0b10u

uvec4 PackTile(uvec2 tile_coord, uint mask, float min_t, float max_t) {
    return uvec4(
        (tile_coord.y << 16u) | tile_coord.x,
        mask,
        floatBitsToUint(min_t),
        floatBitsToUint(max_t)
    );
}

void UnpackTile(uvec4 tile, out uvec2 tile_coord, out uint mask, out float min_t, out float max_t) {
    tile_coord = uvec2(tile.x & 0xffff, (tile.x >> 16) & 0xffff);
    mask = tile.y;
    min_t = uintBitsToFloat(tile.z);
    max_t = uintBitsToFloat(tile.w);
}

uint LaneIdToBitShift(uvec2 id) {
    return id.y * TILE_SIZE_X + id.x;
}

uvec2 GetTileIndexFromPixelPosition(uvec2 pixel_pos) {
    return uvec2(pixel_pos.x / TILE_SIZE_X, pixel_pos.y / TILE_SIZE_Y);
}

uint LinearTileIndex(uvec2 tile_index, uint screen_width) {
    return tile_index.y * ((screen_width + TILE_SIZE_X - 1) / TILE_SIZE_X) + tile_index.x;
}

uint GetBitMaskFromPixelPosition(uvec2 pixel_pos) {
    uint lane_index = (pixel_pos.y % 4u) * 8u + (pixel_pos.x % 8u);
    return (1u << lane_index);
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

#line 19

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

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;
layout(binding = HISTORY_TEX_SLOT) uniform sampler2D g_hist_tex;
layout(binding = PREV_DEPTH_TEX_SLOT) uniform sampler2D g_prev_depth_tex;
layout(binding = PREV_MOMENTS_TEX_SLOT) uniform sampler2D g_prev_moments_tex;

layout(std430, binding = RAY_HITS_BUF_SLOT) readonly buffer RayHits {
    uint g_ray_hits[];
};

layout(std430, binding = OUT_TILE_METADATA_BUF_SLOT) writeonly buffer TileMetadata {
    uint g_tile_metadata[];
};

layout(binding = OUT_REPROJ_RESULTS_IMG_SLOT, rg16f) uniform restrict image2D g_reproj_results_img;
layout(binding = OUT_MOMENTS_IMG_SLOT, r11f_g11f_b10f) uniform restrict image2D g_out_moments_img;

shared int g_false_count;

bool ThreadGroupAllTrue(bool val) {
#ifndef NO_SUBGROUP_EXTENSIONS
    if (gl_SubgroupSize == LOCAL_GROUP_SIZE_X * LOCAL_GROUP_SIZE_Y) {
        return subgroupAll(val);
    } else
#endif
    {
        groupMemoryBarrier(); barrier();
        g_false_count = 0;
        groupMemoryBarrier(); barrier();
        if (!val) {
            g_false_count = 1;
        }
        groupMemoryBarrier(); barrier();
        return g_false_count == 0;
    }
}

bool IsShadowReceiver(uvec2 p) {
    float depth = texelFetch(g_depth_tex, ivec2(p), 0).r;
    return (depth > 0.0) && (depth < 1.0);
}

void WriteTileMetaData(uvec2 gid, uvec2 gtid, bool is_cleared, bool all_in_light) {
    if (all(equal(gtid, uvec2(0)))) {
        uint light_mask = all_in_light ? TILE_META_DATA_LIGHT_MASK : 0u;
        uint clear_mask = is_cleared ? TILE_META_DATA_CLEAR_MASK : 0;
        uint mask = light_mask | clear_mask;

        uint tile_size_x = (g_params.img_size.x + 7) / 8;
        g_tile_metadata[gid.y * tile_size_x + gid.x] = mask;
    }
}

void ClearTargets(uvec2 did, uvec2 gtid, uvec2 gid, float shadow_value, bool is_shadow_receiver, bool all_in_light) {
    WriteTileMetaData(gid, gtid, true, all_in_light);
    imageStore(g_reproj_results_img, ivec2(did), vec4(shadow_value, 0.0, 0.0, 0.0)); // mean, variance

    float temporal_sample_count = is_shadow_receiver ? 1 : 0;
    imageStore(g_out_moments_img, ivec2(did), vec4(shadow_value, 0.0, temporal_sample_count, 0.0)); // mean, variance, temporal sample count
}

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

void SearchSpatialRegion(uvec2 gid, out bool all_in_light, out bool all_in_shadow) {
    // The spatial passes can reach a total region of 1+2+4 = 7x7 around each block.
    // The masks are 8x4, so we need a larger vertical stride

    // Visualization - each x represents a 4x4 block, xx is one entire 8x4 mask as read from the raytracer result
    // Same for yy, these are the ones we are working on right now

    // xx xx xx
    // xx xx xx
    // xx yy xx <-- yy here is the base_tile below
    // xx yy xx
    // xx xx xx
    // xx xx xx

    // All of this should result in scalar ops
    uvec2 base_tile = GetTileIndexFromPixelPosition(gid * ivec2(8, 8));

    // Load the entire region of masks in a scalar fashion
    uint combined_or_mask = 0;
    uint combined_and_mask = 0xFFFFFFFF;
    for (int j = -2; j <= 3; ++j) {
        for (int i = -1; i <= 1; ++i) {
            ivec2 tile_index = ivec2(base_tile) + ivec2(i, j);

            uint ix = (g_params.img_size.x + 7) / 8;
            uint iy = (g_params.img_size.y + 3) / 4;

            tile_index = clamp(tile_index, ivec2(0), ivec2(ix, iy) - 1);
            uint linear_tile_index = LinearTileIndex(tile_index, g_params.img_size.x);
            uint shadow_mask = g_ray_hits[linear_tile_index];

            combined_or_mask = combined_or_mask | shadow_mask;
            combined_and_mask = combined_and_mask & shadow_mask;
        }
    }

    all_in_light = (combined_and_mask == 0xFFFFFFFFu);
    all_in_shadow = (combined_or_mask == 0u);
}

bool IsDisoccluded(uvec2 did, float depth, vec2 velocity) {
    ivec2 dims = ivec2(g_params.img_size);
    vec2 texel_size = g_params.inv_img_size;
    vec2 uv = (vec2(did) + vec2(0.5)) * texel_size;
    vec2 ndc = (2.0 * uv - 1.0) * vec2(1.0, -1.0);
    vec2 previous_uv = uv - velocity;

    bool is_disoccluded = true;
    if (all(greaterThan(previous_uv, vec2(0.0))) && all(lessThan(previous_uv, vec2(1.0)))) {
        // Read the center values
        vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(did), 0)).xyz;

        vec4 clip_space = g_shrd_data.delta_matrix * vec4(ndc, depth, 1.0);
        clip_space /= clip_space.w; // perspective divide

        // How aligned with the view vector? (the more Z aligned, the higher the depth errors)
        vec4 homogeneous = g_shrd_data.inv_proj_matrix * vec4(ndc, depth, 1.0);
        vec3 world_position = homogeneous.xyz / homogeneous.w;  // perspective divide
        vec3 view_direction = normalize(g_shrd_data.cam_pos_and_gamma.xyz - world_position);
        float z_alignment = 1.0 - dot(view_direction, normal);
        z_alignment = pow(z_alignment, 8);

        // Calculate the depth difference
        float linear_depth = LinearizeDepth(clip_space.z, g_shrd_data.clip_info);   // get linear depth

        ivec2 idx = ivec2(previous_uv * dims);
        float previous_depth = LinearizeDepth(texelFetch(g_prev_depth_tex, idx, 0).r, g_shrd_data.clip_info);
        float depth_difference = abs(previous_depth - linear_depth) / linear_depth;

        // Resolve into the disocclusion mask
        float depth_tolerance = mix(1e-2, 1e-1, z_alignment);
        is_disoccluded = depth_difference >= depth_tolerance;
    }

    return is_disoccluded;
}

vec2 GetClosestVelocity(uvec2 did, float depth) {
    vec2 closest_velocity = texelFetch(g_velocity_tex, ivec2(did), 0).rg;
    float closest_depth = depth;

#ifndef NO_SUBGROUP_EXTENSIONS
    float new_depth = subgroupQuadSwapHorizontal(closest_depth);
    vec2 new_velocity = subgroupQuadSwapHorizontal(closest_velocity);
#else
    // TODO: ...
    float new_depth = 0.0;
    vec2 new_velocity = vec2(0.0);
#endif

#ifdef INVERTED_DEPTH_RANGE
    if (new_depth > closest_depth)
#else
    if (new_depth < closest_depth)
#endif
    {
        closest_depth = new_depth;
        closest_velocity = new_velocity;
    }

#ifndef NO_SUBGROUP_EXTENSIONS
    new_depth = subgroupQuadSwapVertical(closest_depth);
    new_velocity = subgroupQuadSwapVertical(closest_velocity);
#else
    // TODO: ...
#endif

#ifdef INVERTED_DEPTH_RANGE
    if (new_depth > closest_depth)
#else
    if (new_depth < closest_depth)
#endif
    {
        closest_depth = new_depth;
        closest_velocity = new_velocity;
    }

    return closest_velocity;
}

#define KERNEL_RADIUS 8
float KernelWeight(float i) {
#define KERNEL_WEIGHT(i) (exp(-3.0 * float(i * i) / ((KERNEL_RADIUS + 1.0) * (KERNEL_RADIUS + 1.0))))

    // Statically initialize kernel_weights_sum
    float kernel_weights_sum = 0;
    kernel_weights_sum += KERNEL_WEIGHT(0);
    for (int c = 1; c <= KERNEL_RADIUS; ++c) {
        kernel_weights_sum += 2 * KERNEL_WEIGHT(c); // Add other half of the kernel to the sum
    }
    float inv_kernel_weights_sum = rcp(kernel_weights_sum);

    // The only runtime code in this function
    return KERNEL_WEIGHT(i) * inv_kernel_weights_sum;
}

void AccumulateMoments(float value, float weight, inout float moments) {
    // We get value from the horizontal neighborhood calculations. Thus, it's both mean and variance due to using one sample per pixel
    moments += value * weight;
}

// The horizontal part of a 17x17 local neighborhood kernel
float HorizontalNeighborhood(ivec2 did) {
    ivec2 base_did = did;

    // Prevent vertical out of bounds access
    if ((base_did.y < 0) || (base_did.y >= g_params.img_size.y)) return 0;

    uvec2 tile_index = GetTileIndexFromPixelPosition(base_did);
    int linear_tile_index = int(LinearTileIndex(tile_index, g_params.img_size.x));

    int left_tile_index = linear_tile_index - 1;
    int center_tile_index = linear_tile_index;
    int right_tile_index = linear_tile_index + 1;

    bool is_first_tile_in_row = tile_index.x == 0;
    bool is_last_tile_in_row = tile_index.x == (((g_params.img_size.x + 7) / 8) - 1);

    uint left_tile = 0;
    if (!is_first_tile_in_row) left_tile = g_ray_hits[left_tile_index];
    uint center_tile = g_ray_hits[center_tile_index];
    uint right_tile = 0;
    if (!is_last_tile_in_row) right_tile = g_ray_hits[right_tile_index];

    // Construct a single uint with the lowest 17bits containing the horizontal part of the local neighborhood.

    // First extract the 8 bits of our row in each of the neighboring tiles
    uint row_base_index = (did.y % 4) * 8;
    uint left = (left_tile >> row_base_index) & 0xFF;
    uint center = (center_tile >> row_base_index) & 0xFF;
    uint right = (right_tile >> row_base_index) & 0xFF;

    // Combine them into a single mask containting [left, center, right] from least significant to most significant bit
    uint neighborhood = left | (center << 8) | (right << 16);

    // Make sure our pixel is at bit position 9 to get the highest contribution from the filter kernel
    const uint bit_index_in_row = (did.x % 8);
    neighborhood = neighborhood >> bit_index_in_row; // Shift out bits to the right, so the center bit ends up at bit 9.

    float moment = 0.0; // For one sample per pixel this is both, mean and variance

    // First 8 bits up to the center pixel
    uint mask;
    int i;
    for (i = 0; i < 8; ++i) {
        mask = 1u << i;
        moment += (mask & neighborhood) != 0u ? KernelWeight(8 - i) : 0;
    }

    // Center pixel
    mask = 1u << 8;
    moment += (mask & neighborhood) != 0u ? KernelWeight(0) : 0;

    // Last 8 bits
    for (i = 1; i <= 8; ++i) {
        mask = 1u << (8 + i);
        moment += (mask & neighborhood) != 0u ? KernelWeight(i) : 0;
    }

    return moment;
}

shared float g_neighborhood[8][24];

float ComputeLocalNeighborhood(ivec2 did, ivec2 gtid) {
    float local_neighborhood = 0;

    float upper = HorizontalNeighborhood(ivec2(did.x, did.y - 8));
    float center = HorizontalNeighborhood(ivec2(did.x, did.y));
    float lower = HorizontalNeighborhood(ivec2(did.x, did.y + 8));

    g_neighborhood[gtid.x][gtid.y] = upper;
    g_neighborhood[gtid.x][gtid.y + 8] = center;
    g_neighborhood[gtid.x][gtid.y + 16] = lower;

    groupMemoryBarrier(); barrier();

    // First combine the own values.
    // KERNEL_RADIUS pixels up is own upper and KERNEL_RADIUS pixels down is own lower value
    AccumulateMoments(center, KernelWeight(0), local_neighborhood);
    AccumulateMoments(upper, KernelWeight(KERNEL_RADIUS), local_neighborhood);
    AccumulateMoments(lower, KernelWeight(KERNEL_RADIUS), local_neighborhood);

    // Then read the neighboring values.
    for (int i = 1; i < KERNEL_RADIUS; ++i) {
        float upper_value = g_neighborhood[gtid.x][8 + gtid.y - i];
        float lower_value = g_neighborhood[gtid.x][8 + gtid.y + i];
        float weight = KernelWeight(i);
        AccumulateMoments(upper_value, weight, local_neighborhood);
        AccumulateMoments(lower_value, weight, local_neighborhood);
    }

    return local_neighborhood;
}

void TileClassification(uint group_index, uvec2 gid) {
    uvec2 gtid = RemapLane8x8(group_index);
    uvec2 did = gid * 8 + gtid;

    bool is_shadow_receiver = IsShadowReceiver(did);

    bool skip = ThreadGroupAllTrue(!is_shadow_receiver);
    if (skip) {
        ClearTargets(did, gtid, gid, 0, is_shadow_receiver, false);
        return;
    }

    bool all_in_light = false;
    bool all_in_shadow = false;
    SearchSpatialRegion(gid, all_in_light, all_in_shadow);
    float shadow_value = all_in_light ? 1 : 0; // Either all_in_light or all_in_shadow must be true, otherwise we would not skip the tile.

    bool can_skip = all_in_light || all_in_shadow;
    // We have to append the entire tile if there is a single lane that we can't skip
    bool skip_tile = ThreadGroupAllTrue(can_skip);
    if (skip_tile) {
        // We have to set all resources of the tile we skipped to sensible values as neighboring active denoiser tiles might want to read them.
        ClearTargets(did, gtid, gid, shadow_value, is_shadow_receiver, all_in_light);
        return;
    }

    WriteTileMetaData(gid, gtid, false, false);

    float depth = texelFetch(g_depth_tex, ivec2(did), 0).r;
    vec2 velocity = GetClosestVelocity(did.xy, depth); // Must happen before we deactivate lanes

    float local_neighborhood = ComputeLocalNeighborhood(ivec2(did), ivec2(gtid));

    vec2 texel_size = g_params.inv_img_size;
    vec2 uv = (vec2(did.xy) + vec2(0.5)) * texel_size;
    vec2 history_uv = uv - velocity;
    ivec2 history_pos = ivec2(history_uv * g_params.img_size);

    uvec2 tile_index = GetTileIndexFromPixelPosition(did);
    uint linear_tile_index = LinearTileIndex(tile_index, g_params.img_size.x);

    uint shadow_tile = g_ray_hits[linear_tile_index];

    vec3 moments_current = vec3(0.0);
    float variance = 0;
    float shadow_clamped = 0;

    if (is_shadow_receiver) { // do not process sky pixels
        bool hit_light = (shadow_tile & GetBitMaskFromPixelPosition(did)) != 0u;
        float shadow_current = hit_light ? 1.0 : 0.0;

        { // Perform moments and variance calculations
            bool is_disoccluded = IsDisoccluded(did, depth, velocity);
            vec3 previous_moments = is_disoccluded ? vec3(0.0, 0.0, 0.0) // Can't trust previous moments on disocclusion
                                                   //: texelFetch(g_prev_moments_tex, history_pos, 0).xyz;
                                                   : textureLod(g_prev_moments_tex, history_uv, 0.0).xyz;

            float old_m = previous_moments.x;
            float old_s = previous_moments.y;
            float sample_count = previous_moments.z + 1.0;
            float new_m = old_m + (shadow_current - old_m) / sample_count;
            float new_s = old_s + (shadow_current - old_m) * (shadow_current - new_m);

            variance = (sample_count > 1.0 ? new_s / (sample_count - 1.0) : 1.0);
            moments_current = vec3(new_m, new_s, sample_count);
        }

        { // Retrieve local neighborhood and reproject
            float mean = local_neighborhood;
            float spatial_variance = local_neighborhood;

            spatial_variance = max(spatial_variance - mean * mean, 0.0);

            // Compute the clamping bounding box
            const float std_deviation = sqrt(spatial_variance);
            const float nmin = mean - 0.5 * std_deviation;
            const float nmax = mean + 0.5 * std_deviation;

            // Clamp reprojected sample to local neighborhood
            float shadow_previous = shadow_current;
            if (/*FFX_DNSR_Shadows_IsFirstFrame() == 0*/ true) {
                //shadow_previous = FFX_DNSR_Shadows_ReadHistory(history_uv);
                shadow_previous = textureLod(g_hist_tex, history_uv, 0.0).r;
            }

            shadow_clamped = clamp(shadow_previous, nmin, nmax);

            // Reduce history weighting
            const float sigma = 20.0;
            float temporal_discontinuity = (shadow_previous - mean) / max(0.5 * std_deviation, 0.001);
            float sample_counter_damper = exp(-temporal_discontinuity * temporal_discontinuity / sigma);
            moments_current.z *= sample_counter_damper;

            // Boost variance on first frames
            if (moments_current.z < 16.0) {
                float variance_boost = max(16.0 - moments_current.z, 1.0);
                variance = max(variance, spatial_variance);
                variance *= variance_boost;
            }
        }

        // Perform the temporal blend
#if 1 // original code
        float history_weight = sqrt(max(8.0 - moments_current.z, 0.0) / 8.0);
        shadow_clamped = mix(shadow_clamped, shadow_current, mix(0.05, 1.0, history_weight));
#else // linear accumulation
        float accumulation_speed = 1.0 / max(moments_current.z, 1.0);
        float weight = (1.0 - accumulation_speed);
        shadow_clamped = mix(shadow_current, shadow_clamped, weight);
#endif
    }

    // Output the results of the temporal pass
    imageStore(g_reproj_results_img, ivec2(did.xy), vec4(shadow_clamped, variance, 0.0, 0.0));
    imageStore(g_out_moments_img, ivec2(did.xy), vec4(moments_current, 0.0));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uvec2 group_id = gl_WorkGroupID.xy;
    uint group_index = gl_LocalInvocationIndex;
    TileClassification(group_index, group_id);
}
