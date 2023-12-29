#version 430

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

#line 4
#line 0
#ifndef SKINNING_INTERFACE_H
#define SKINNING_INTERFACE_H

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

INTERFACE_START(Skinning)

struct Params {
    UVEC4_TYPE uSkinParams;
    UVEC4_TYPE uShapeParamsCurr;
    UVEC4_TYPE uShapeParamsPrev;
};

#define LOCAL_GROUP_SIZE 128

#define IN_VERTICES_SLOT 0
#define IN_MATRICES_SLOT 1
#define IN_SHAPE_KEYS_SLOT 2
#define IN_DELTAS_SLOT 3
#define OUT_VERTICES0 4
#define OUT_VERTICES1 5

INTERFACE_END

#endif // SKINNING_INTERFACE_H

#line 5

/*
UNIFORM_BLOCKS
    UniformParams : 24
*/

struct InVertex {
    highp vec4 p_and_nxy;
    highp uvec2 nz_and_b;
    highp uvec2 t0_and_t1;
    highp uvec2 bone_indices;
    highp uvec2 bone_weights;
};

struct InDelta {
    highp vec2 dpxy;
    highp vec2 dpz_dnxy;
    highp uvec2 dnz_and_db;
};

struct OutVertexData0 {
    highp vec4 p_and_t0;
};

struct OutVertexData1 {
    highp uvec2 n_and_bx;
    highp uvec2 byz_and_t1;
};

layout(std430, binding = IN_VERTICES_SLOT) readonly buffer Input0 {
    InVertex vertices[];
} g_in_data0;

layout(std430, binding = IN_MATRICES_SLOT) readonly buffer Input1 {
    highp mat3x4 matrices[];
} g_in_data1;

layout(std430, binding = IN_SHAPE_KEYS_SLOT) readonly buffer Input2 {
    highp uint shape_keys[];
} g_in_data2;

layout(std430, binding = IN_DELTAS_SLOT) readonly buffer Input3 {
    InDelta deltas[];
} g_in_data3;

layout(std430, binding = OUT_VERTICES0) writeonly buffer Output0 {
    OutVertexData0 vertices[];
} g_out_data0;

layout(std430, binding = OUT_VERTICES1) writeonly buffer Output1 {
    OutVertexData1 vertices[];
} g_out_data1;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (local_size_x = LOCAL_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.uSkinParams.y) {
        return;
    }

    highp uint in_ndx = g_params.uSkinParams.x + gl_GlobalInvocationID.x;
    mediump uint xform_offset = g_params.uSkinParams.z;

    highp vec3 p = g_in_data0.vertices[in_ndx].p_and_nxy.xyz;

    highp uint _nxy = floatBitsToUint(g_in_data0.vertices[in_ndx].p_and_nxy.w);
    highp vec2 nxy = unpackSnorm2x16(_nxy);

    highp uint _nz_and_bx = g_in_data0.vertices[in_ndx].nz_and_b.x;
    highp vec2 nz_and_bx = unpackSnorm2x16(_nz_and_bx);

    highp uint _byz = g_in_data0.vertices[in_ndx].nz_and_b.y;
    highp vec2 byz = unpackSnorm2x16(_byz);

    highp vec3 n = vec3(nxy, nz_and_bx.x), b = vec3(nz_and_bx.y, byz);

    highp vec3 p2 = p;

    for (uint j = g_params.uShapeParamsCurr.x; j < g_params.uShapeParamsCurr.x + g_params.uShapeParamsCurr.y; j++) {
        highp uint shape_data = g_in_data2.shape_keys[j];
        mediump uint shape_index = bitfieldExtract(shape_data, 0, 16);
        mediump float shape_weight = unpackUnorm2x16(shape_data).y;

        int sh_i = int(g_params.uShapeParamsCurr.z + shape_index * g_params.uSkinParams.y + gl_GlobalInvocationID.x);
        p += shape_weight * vec3(g_in_data3.deltas[sh_i].dpxy, g_in_data3.deltas[sh_i].dpz_dnxy.x);
        highp uint _dnxy = floatBitsToUint(g_in_data3.deltas[sh_i].dpz_dnxy.y);
        mediump vec2 _dnz_and_dbx = unpackSnorm2x16(g_in_data3.deltas[sh_i].dnz_and_db.x);
        n += shape_weight * vec3(unpackSnorm2x16(_dnxy), _dnz_and_dbx.x);
        mediump vec2 _dbyz = unpackSnorm2x16(g_in_data3.deltas[sh_i].dnz_and_db.y);
        b += shape_weight * vec3(_dnz_and_dbx.y, _dbyz);
    }

    for (uint j = g_params.uShapeParamsPrev.x; j < g_params.uShapeParamsPrev.x + g_params.uShapeParamsPrev.y; j++) {
        highp uint shape_data = g_in_data2.shape_keys[j];
        mediump uint shape_index = bitfieldExtract(shape_data, 0, 16);
        mediump float shape_weight = unpackUnorm2x16(shape_data).y;

        int sh_i = int(g_params.uShapeParamsPrev.z + shape_index * g_params.uSkinParams.y + gl_GlobalInvocationID.x);
        p2 += shape_weight * vec3(g_in_data3.deltas[sh_i].dpxy, g_in_data3.deltas[sh_i].dpz_dnxy.x);
    }

    mediump uvec4 bone_indices = uvec4(
        bitfieldExtract(g_in_data0.vertices[in_ndx].bone_indices.x, 0, 16),
        bitfieldExtract(g_in_data0.vertices[in_ndx].bone_indices.x, 16, 16),
        bitfieldExtract(g_in_data0.vertices[in_ndx].bone_indices.y, 0, 16),
        bitfieldExtract(g_in_data0.vertices[in_ndx].bone_indices.y, 16, 16)
    );
    mediump vec4 bone_weights = vec4(
        unpackUnorm2x16(g_in_data0.vertices[in_ndx].bone_weights.x),
        unpackUnorm2x16(g_in_data0.vertices[in_ndx].bone_weights.y)
    );

    highp mat3x4 mat_curr = mat3x4(0.0);
    highp mat3x4 mat_prev = mat3x4(0.0);

    for (int j = 0; j < 4; j++) {
        if (bone_weights[j] > 0.0) {
            mat_curr += g_in_data1.matrices[2u * (xform_offset + bone_indices[j]) + 0u] * bone_weights[j];
            mat_prev += g_in_data1.matrices[2u * (xform_offset + bone_indices[j]) + 1u] * bone_weights[j];
        }
    }

    highp mat4x3 tr_mat_curr = transpose(mat_curr);

    highp vec3 p_curr = tr_mat_curr * vec4(p, 1.0);
    mediump vec3 n_curr = tr_mat_curr * vec4(n, 0.0);
    mediump vec3 b_curr = tr_mat_curr * vec4(b, 0.0);

    highp uint out_ndx_curr = g_params.uSkinParams.w + gl_GlobalInvocationID.x;

    g_out_data0.vertices[out_ndx_curr].p_and_t0.xyz = p_curr;
    // copy texture coordinates unchanged
    g_out_data0.vertices[out_ndx_curr].p_and_t0.w = uintBitsToFloat(g_in_data0.vertices[in_ndx].t0_and_t1.x);

    g_out_data1.vertices[out_ndx_curr].n_and_bx.x = packSnorm2x16(n_curr.xy);
    g_out_data1.vertices[out_ndx_curr].n_and_bx.y = packSnorm2x16(vec2(n_curr.z, b_curr.x));
    g_out_data1.vertices[out_ndx_curr].byz_and_t1.x = packSnorm2x16(b_curr.yz);
    // copy texture coordinates unchanged
    g_out_data1.vertices[out_ndx_curr].byz_and_t1.y = g_in_data0.vertices[in_ndx].t0_and_t1.y;

    highp mat4x3 tr_mat_prev = transpose(mat_prev);
    highp vec3 p_prev = tr_mat_prev * vec4(p2, 1.0);

    highp uint out_ndx_prev = out_ndx_curr + uint(REN_MAX_SKIN_VERTICES_TOTAL);
    g_out_data0.vertices[out_ndx_prev].p_and_t0.xyz = p_prev;
}
