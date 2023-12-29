#version 430
#extension GL_EXT_texture_buffer : enable
#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#line 0

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

#define INSTANCE_BUF_STRIDE 12

#define FetchModelMatrix(instance_buf, instance)                                        \
    transpose(mat4(texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 0),    \
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 1),    \
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 2),    \
                   vec4(0.0, 0.0, 0.0, 1.0)))

#define FetchNormalMatrix(instance_buf, instance)                                       \
    mat4(texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 4),              \
         texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 5),              \
         texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 6),              \
         vec4(0.0, 0.0, 0.0, 1.0))

#define FetchPrevModelMatrix(instance_buf, instance)                                    \
    transpose(mat4(texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 8),    \
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 9),    \
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 10),   \
                   vec4(0.0, 0.0, 0.0, 1.0)))

#line 8
#line 0

#if !defined(VULKAN)
layout(location = REN_U_BASE_INSTANCE_LOC) uniform uint _base_instance;
#define gl_InstanceIndex (_base_instance + gl_InstanceID)
#endif

#line 9
#line 0
#ifndef _TEXTURING_GLSL
#define _TEXTURING_GLSL

#if defined(VULKAN) || defined(GL_ARB_bindless_texture)
#define BINDLESS_TEXTURES
#endif

#if defined(VULKAN)
#extension GL_EXT_nonuniform_qualifier : enable
    #define TEX_HANDLE uint
    #define SAMPLER2D(ndx) scene_textures[nonuniformEXT(ndx)]
    #define GET_HANDLE

    layout(set = REN_SET_SCENETEXTURES, binding = REN_BINDLESS_TEX_SLOT) uniform sampler2D scene_textures[];
#else // VULKAN
    #if defined(GL_ARB_bindless_texture)
        #define TEX_HANDLE uvec2
        #define SAMPLER2D(ndx) sampler2D(ndx)
        #define GET_HANDLE(ndx) texture_handles[ndx]

        layout(binding = REN_BINDLESS_TEX_SLOT, std430) readonly buffer TextureHandles {
            uvec2 texture_handles[];
        };
    #else // GL_ARB_bindless_texture
        #define SAMPLER2D
    #endif // GL_ARB_bindless_texture
#endif // VULKAN

#endif // _TEXTURING_GLSL

#line 10
#line 0
#ifndef _VEGETATION_GLSL
#define _VEGETATION_GLSL

#define PP_RES_X 32
#define PP_RES_Y 64

#define PP_EXTENT 20.48

#define MAX_ALLOWED_HIERARCHY 4

#define VEGE_NOISE_SCALE_LF (1.0 / 1536.0)
#define VEGE_NOISE_SCALE_HF (1.0 / 8.0)

const float WindNoiseSpeedBase = 0.1;
const float WindNoiseSpeedLevelMul = 1.666;

const float WindAngleRotBase = 5;
const float WindAngleRotLevelMul = 10;
const float WindAngleRotMax = 60;

const float WindParentContrib = 0.25;

const float WindMotionDampBase = 3;
const float WindMotionDampLevelMul = 0.8;

vec2 get_uv_by_index(uint index) {
    vec2 idx2D = vec2(index % PP_RES_X, index / PP_RES_X);

    const vec2 invRes = 1.0 / vec2(PP_RES_X, PP_RES_Y);
    return (idx2D + 0.5) * invRes;
}

int get_index_by_uv(vec2 uv) {
    ivec2 idx2D = ivec2(floor(uv * ivec2(PP_RES_X, PP_RES_Y)));

    return idx2D[0] + idx2D[1] * PP_RES_X;
}

int get_parent_index_and_pivot_pos(sampler2D pp_pos_tex, vec2 coords, out vec3 pivot_pos) {
    vec4 result = textureLod(pp_pos_tex, coords, 0.0);

    pivot_pos = result.xyz;

    // Unpack int from float
    int idx = int(result.w);

    return idx;
}

vec4 get_parent_pivot_direction_and_extent(sampler2D pp_dir_tex, vec2 coords) {
    vec4 result = textureLod(pp_dir_tex, coords, 0.0);
    result.xyz = result.xyz * 2.0 - 1.0;

    result.xyz = normalize(result.xyz);
    result.w *= PP_EXTENT;

    return result;
}

struct HierarchyData {
    // Maximal hierarchy level found
    int max_hierarchy_level;

    // Hierarchical attributes, 0 == current level, 1 == parent, 2 == parent of parent, etc.
    vec3 branch_pivot_pos[MAX_ALLOWED_HIERARCHY];
    vec4 branch_dir_extent[MAX_ALLOWED_HIERARCHY];
};

HierarchyData FetchHierarchyData(sampler2D pp_pos_tex, sampler2D pp_dir_tex, vec2 start_coords) {
    HierarchyData data;
    data.max_hierarchy_level = 0;

    vec2 current_uv = start_coords;
    // Iterate starting from SELF, as in HierarchyData
    for(int level = 0; level < MAX_ALLOWED_HIERARCHY; ++level) {
        int cur_idx = get_index_by_uv(current_uv);
        int parent_idx = get_parent_index_and_pivot_pos(pp_pos_tex, current_uv, data.branch_pivot_pos[level].xyz);
        data.branch_dir_extent[level] = get_parent_pivot_direction_and_extent(pp_dir_tex, current_uv);

        // False will happen at trunk level
        if (cur_idx != parent_idx) {
            data.max_hierarchy_level++;

            // Next level UVs
            current_uv = get_uv_by_index(parent_idx);
        }
    }

    return data;
}

// Geometric progression based on hierarchy level
// Probably good idea to normalize to (0, maxAllowedHierarchy) range
float get_wind_speed_mult(int idx) {
    return WindNoiseSpeedBase * pow(WindNoiseSpeedLevelMul, idx);
}

float get_angle_rotation_rate(int idx) {
    return WindAngleRotBase + WindAngleRotLevelMul * idx;
}

float get_motion_dampening_falloff_radius(int idx) {
    return WindMotionDampBase * pow(WindMotionDampLevelMul, idx);
}

vec4 make_quat(vec3 axis, float angle) {
    return vec4(axis.x * sin(angle / 2), axis.y * sin(angle / 2), axis.z * sin(angle / 2), cos(angle / 2));
}

vec3 quat_rotate(vec4 q, vec3 v) {
    vec3 t = 2 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

vec3 TransformVegetation(vec3 old_pos, inout vec3 inout_normal, inout vec3 inout_tangent, sampler2D noise_tex,
                         vec4 wind_scroll, vec4 wind_params, vec4 wind_vec_ls, HierarchyData data) {
    mat2 uv_tr;
    uv_tr[0] = normalize(vec2(wind_vec_ls.x, wind_vec_ls.z));
    uv_tr[1] = vec2(uv_tr[0].y, -uv_tr[0].x);

    float parent_angle_animation = 0.0;
    vec3 pos_offset = vec3(0.0);
    vec3 normal = inout_normal, tangent = inout_tangent;

    // Iterate starting from trunk at 0
    for(int idx = 0; idx <= data.max_hierarchy_level; ++idx) {
        // Because HierarchyData and this loop have reversed ordering, we need reversed index
        int revIdx = max(data.max_hierarchy_level - idx, 0);

        vec3 branch_pivot_pos = data.branch_pivot_pos[revIdx];
        vec3 branch_dir = data.branch_dir_extent[revIdx].xyz;
        float branch_extent = data.branch_dir_extent[revIdx].w;

        vec3 branch_tip_pos = branch_pivot_pos + branch_dir * branch_extent;
        vec2 branch_tip_pos_ts = uv_tr * branch_tip_pos.xz + vec2(2.0 * branch_tip_pos.y, 0.0);

        vec3 wind_dir = get_wind_speed_mult(idx) * (wind_vec_ls.xyz + wind_vec_ls.w * textureLod(noise_tex, wind_scroll.xy + branch_tip_pos_ts, 0.0).rgb);
        float wind_speed = length(wind_dir);

        float rotation_angle_animation = 0;
        rotation_angle_animation += wind_speed * get_angle_rotation_rate(idx);
        rotation_angle_animation += parent_angle_animation * WindParentContrib;
        rotation_angle_animation = min(rotation_angle_animation, WindAngleRotMax);

        vec3 dir_from_root = (old_pos - branch_pivot_pos);
        float along_branch = dot(dir_from_root, branch_dir);
        float falloff = get_motion_dampening_falloff_radius(idx) * branch_extent;
        float motion_mask = saturate(along_branch / falloff);

        float rotationAngle = rotation_angle_animation * motion_mask;

        vec3 rotation_axis = cross(branch_dir, wind_dir);
        //Protect cross-product from returning 0
        rotation_axis = mix(branch_dir, rotation_axis, saturate(wind_speed / 0.001));
        rotation_axis = normalize(rotation_axis);

        vec4 rot_quat = make_quat(rotation_axis, rotationAngle * M_PI / 180);
        vec3 new_pos = quat_rotate(rot_quat, old_pos - branch_pivot_pos) + branch_pivot_pos;

        normal = quat_rotate(rot_quat, normal);
        tangent = quat_rotate(rot_quat, tangent);
        pos_offset += new_pos - old_pos;
        parent_angle_animation += rotation_angle_animation;
    }

    inout_normal = normal;
    inout_tangent = tangent;

    return old_pos + pos_offset;
}

#endif // _VEGETATION_GLSL

#line 11

#line 0
#ifndef SHADOW_INTERFACE_H
#define SHADOW_INTERFACE_H

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

INTERFACE_START(Shadow)

struct Params {
    MAT4_TYPE g_shadow_view_proj_mat;
};

#define U_M_MATRIX_LOC 12

INTERFACE_END

#endif // SHADOW_INTERFACE_H

#line 13

/*
PERM @TRANSPARENT_PERM
*/

/*
UNIFORM_BLOCKS
    SharedDataBlock : 23
*/

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(location = REN_VTX_POS_LOC) in vec3 g_in_vtx_pos;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 g_in_vtx_uvs0;
#endif
layout(location = REN_VTX_AUX_LOC) in uint g_in_vtx_uvs1_packed;

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer g_instances_buf;
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;

layout(binding = REN_INST_INDICES_BUF_SLOT, std430) readonly buffer InstanceIndices {
    ivec2 g_instance_indices[];
};

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    mat4 g_shadow_view_proj_mat;
};
#else // VULKAN
layout(location = U_M_MATRIX_LOC) uniform mat4 g_shadow_view_proj_mat;
#endif // VULKAN

layout(binding = REN_MATERIALS_SLOT, std430) readonly buffer Materials {
    MaterialData g_materials[];
};

#if !defined(BINDLESS_TEXTURES)
layout(binding = REN_MAT_TEX4_SLOT) uniform sampler2D g_pp_pos_tex;
layout(binding = REN_MAT_TEX5_SLOT) uniform sampler2D g_pp_dir_tex;
#endif

#ifdef TRANSPARENT_PERM
    LAYOUT(location = 0) out vec2 g_vtx_uvs0;
    #if defined(BINDLESS_TEXTURES)
        LAYOUT(location = 1) out flat TEX_HANDLE g_alpha_tex;
    #endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

void main() {
    ivec2 instance = g_instance_indices[gl_InstanceIndex];
    mat4 MMatrix = FetchModelMatrix(g_instances_buf, instance.x);

    vec4 veg_params = texelFetch(g_instances_buf, instance.x * INSTANCE_BUF_STRIDE + 3);
    vec2 pp_vtx_uvs = unpackHalf2x16(g_in_vtx_uvs1_packed);

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = g_materials[instance.y];
    TEX_HANDLE g_pp_pos_tex = GET_HANDLE(mat.texture_indices[4]);
    TEX_HANDLE g_pp_dir_tex = GET_HANDLE(mat.texture_indices[5]);
#endif // BINDLESS_TEXTURES
    HierarchyData hdata = FetchHierarchyData(SAMPLER2D(g_pp_pos_tex), SAMPLER2D(g_pp_dir_tex), pp_vtx_uvs);

    vec3 obj_pos_ws = MMatrix[3].xyz;
    vec4 wind_scroll = g_shrd_data.wind_scroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
    vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
    vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vec3 _unused;
    vec3 vtx_pos_ls = TransformVegetation(g_in_vtx_pos, _unused, _unused, g_noise_tex, wind_scroll, wind_params, wind_vec_ls, hdata);

    vec3 vtx_pos_ws = (MMatrix * vec4(vtx_pos_ls, 1.0)).xyz;

#ifdef TRANSPARENT_PERM
    g_vtx_uvs0 = g_in_vtx_uvs0;

#if defined(BINDLESS_TEXTURES)
    g_alpha_tex = GET_HANDLE(mat.texture_indices[3]);
#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

    gl_Position = g_shadow_view_proj_mat * vec4(vtx_pos_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
