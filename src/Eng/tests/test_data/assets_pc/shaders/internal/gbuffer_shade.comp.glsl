#version 430

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
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

#if defined(VULKAN)
#define GetCellIndex(ix, iy, slice, res) \
    (slice * REN_GRID_RES_X * REN_GRID_RES_Y + ((int(res.y) - 1 - iy) * REN_GRID_RES_Y / int(res.y)) * REN_GRID_RES_X + ix * REN_GRID_RES_X / int(res.x))
#else
#define GetCellIndex(ix, iy, slice, res) \
    (slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / int(res.y)) * REN_GRID_RES_X + ix * REN_GRID_RES_X / int(res.x))
#endif

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

const vec2 poisson_disk[16] = vec2[16](
    vec2(-0.5, 0.0),
    vec2(0.0, 0.5),
    vec2(0.5, 0.0),
    vec2(0.0, -0.5),

    vec2(0.0, 0.0),
    vec2(-0.1, -0.32),
    vec2(0.17, 0.31),
    vec2(0.35, 0.04),

    vec2(0.07, 0.7),
    vec2(-0.72, 0.09),
    vec2(0.73, 0.05),
    vec2(0.1, -0.71),

    vec2(0.72, 0.8),
    vec2(-0.75, 0.74),
    vec2(-0.8, -0.73),
    vec2(0.75, -0.81)
);

float SampleShadowPCF5x5(sampler2DShadow g_shadow_tex, highp vec3 shadow_coord) {
    // http://the-witness.net/news/2013/09/shadow-mapping-summary-part-1/

    const highp vec2 shadow_size = vec2(float(REN_SHAD_RES), float(REN_SHAD_RES) / 2.0);
    const highp vec2 shadow_size_inv = vec2(1.0) / shadow_size;

    float z = shadow_coord.z;
    highp vec2 uv = shadow_coord.xy * shadow_size;
    highp vec2 base_uv = floor(uv + 0.5);
    float s = (uv.x + 0.5 - base_uv.x);
    float t = (uv.y + 0.5 - base_uv.y);
    base_uv -= vec2(0.5);
    base_uv *= shadow_size_inv;

    float uw0 = (4.0 - 3.0 * s);
    const float uw1 = 7.0;
    float uw2 = (1.0 + 3.0 * s);

    float u0 = (3.0 - 2.0 * s) / uw0 - 2.0;
    float u1 = (3.0 + s) / uw1;
    float u2 = s / uw2 + 2.0;

    float vw0 = (4.0 - 3.0 * t);
    const float vw1 = 7.0;
    float vw2 = (1.0 + 3.0 * t);

    float v0 = (3.0 - 2.0 * t) / vw0 - 2.0;
    float v1 = (3.0 + t) / vw1;
    float v2 = t / vw2 + 2.0;

    float sum = 0.0;

    u0 = u0 * shadow_size_inv.x + base_uv.x;
    v0 = v0 * shadow_size_inv.y + base_uv.y;

    u1 = u1 * shadow_size_inv.x + base_uv.x;
    v1 = v1 * shadow_size_inv.y + base_uv.y;

    u2 = u2 * shadow_size_inv.x + base_uv.x;
    v2 = v2 * shadow_size_inv.y + base_uv.y;

    sum += uw0 * vw0 * texture(g_shadow_tex, vec3(u0, v0, z));
    sum += uw1 * vw0 * texture(g_shadow_tex, vec3(u1, v0, z));
    sum += uw2 * vw0 * texture(g_shadow_tex, vec3(u2, v0, z));

    sum += uw0 * vw1 * texture(g_shadow_tex, vec3(u0, v1, z));
    sum += uw1 * vw1 * texture(g_shadow_tex, vec3(u1, v1, z));
    sum += uw2 * vw1 * texture(g_shadow_tex, vec3(u2, v1, z));

    sum += uw0 * vw2 * texture(g_shadow_tex, vec3(u0, v2, z));
    sum += uw1 * vw2 * texture(g_shadow_tex, vec3(u1, v2, z));
    sum += uw2 * vw2 * texture(g_shadow_tex, vec3(u2, v2, z));

    sum *= (1.0 / 144.0);

    return sum * sum;
}

float GetSunVisibility(float frag_depth, sampler2DShadow g_shadow_tex, in highp vec3 aVertexShUVs[4]) {
    float visibility = 0.0;

    /*[[branch]]*/ if (frag_depth < REN_SHAD_CASCADE0_DIST) {
        visibility = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[0]);

#if REN_SHAD_CASCADE_SOFT
        /*[[branch]]*/ if (frag_depth > 0.9 * REN_SHAD_CASCADE0_DIST) {
            float v2 = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[1]);

            float k = 10.0 * (frag_depth / REN_SHAD_CASCADE0_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else /*[[branch]]*/ if (frag_depth < REN_SHAD_CASCADE1_DIST) {
        visibility = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[1]);

#if REN_SHAD_CASCADE_SOFT
        /*[[branch]]*/ if (frag_depth > 0.9 * REN_SHAD_CASCADE1_DIST) {
            float v2 = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[2]);

            float k = 10.0 * (frag_depth / REN_SHAD_CASCADE1_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else /*[[branch]]*/ if (frag_depth < REN_SHAD_CASCADE2_DIST) {
        visibility = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[2]);

#if REN_SHAD_CASCADE_SOFT
        /*[[branch]]*/ if (frag_depth > 0.9 * REN_SHAD_CASCADE2_DIST) {
            float v2 = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[3]);

            float k = 10.0 * (frag_depth / REN_SHAD_CASCADE2_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else /*[[branch]]*/ if (frag_depth < REN_SHAD_CASCADE3_DIST) {
        visibility = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[3]);

        float t = smoothstep(0.95 * REN_SHAD_CASCADE3_DIST, REN_SHAD_CASCADE3_DIST, frag_depth);
        visibility = mix(visibility, 1.0, t);
    } else {
        // use direct sun lightmap?
        visibility = 1.0;
    }

    return visibility;
}

float GetSunVisibility(float frag_depth, sampler2DShadow g_shadow_tex, in highp mat4x3 aVertexShUVs) {
    float visibility = 0.0;

    /*[[branch]]*/ if (frag_depth < REN_SHAD_CASCADE0_DIST) {
        visibility = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[0]);

#if REN_SHAD_CASCADE_SOFT
        /*[[branch]]*/ if (frag_depth > 0.9 * REN_SHAD_CASCADE0_DIST) {
            float v2 = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[1]);

            float k = 10.0 * (frag_depth / REN_SHAD_CASCADE0_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else /*[[branch]]*/ if (frag_depth < REN_SHAD_CASCADE1_DIST) {
        visibility = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[1]);

#if REN_SHAD_CASCADE_SOFT
        /*[[branch]]*/ if (frag_depth > 0.9 * REN_SHAD_CASCADE1_DIST) {
            float v2 = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[2]);

            float k = 10.0 * (frag_depth / REN_SHAD_CASCADE1_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else /*[[branch]]*/ if (frag_depth < REN_SHAD_CASCADE2_DIST) {
        visibility = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[2]);

#if REN_SHAD_CASCADE_SOFT
        /*[[branch]]*/ if (frag_depth > 0.9 * REN_SHAD_CASCADE2_DIST) {
            float v2 = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[3]);

            float k = 10.0 * (frag_depth / REN_SHAD_CASCADE2_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else /*[[branch]]*/ if (frag_depth < REN_SHAD_CASCADE3_DIST) {
        visibility = SampleShadowPCF5x5(g_shadow_tex, aVertexShUVs[3]);

        float t = smoothstep(0.95 * REN_SHAD_CASCADE3_DIST, REN_SHAD_CASCADE3_DIST, frag_depth);
        visibility = mix(visibility, 1.0, t);
    } else {
        // use direct sun lightmap?
        visibility = 1.0;
    }

    return visibility;
}

vec3 EvaluateSH(in vec3 normal, in vec4 sh_coeffs[3]) {
    const float SH_A0 = 0.886226952; // PI / sqrt(4.0 * Pi)
    const float SH_A1 = 1.02332675;  // sqrt(PI / 3.0)

    vec4 vv = vec4(SH_A0, SH_A1 * normal.yzx);

    return vec3(dot(sh_coeffs[0], vv), dot(sh_coeffs[1], vv), dot(sh_coeffs[2], vv));
}

#if 0
void GenerateMoments(float depth, float transmittance, out float b_0, out vec4 b) {
    float absorbance = -log(transmittance);

    float depth_pow2 = depth * depth;
    float depth_pow4 = depth_pow2 * depth_pow2;

    b_0 = absorbance;
    b = vec4(depth, depth_pow2, depth_pow2 * depth, depth_pow4) * absorbance;
}

float ComputeTransmittanceAtDepthFrom4PowerMoments(float b_0, vec4 b, float depth, float bias, float overestimation, vec4 bias_vector) {
    // Bias input data to avoid artifacts
    b = mix(b, bias_vector, bias);
    vec3 z;
    z[0] = depth;

    // Compute a Cholesky factorization of the Hankel matrix B storing only non-
    // trivial entries or related products
    float L21D11 = mad(-b[0], b[1], b[2]);
    float D11 = mad(-b[0],b[0], b[1]);
    float InvD11 = 1.0 / D11;
    float L21 = L21D11 * InvD11;
    float SquaredDepthVariance = mad(-b[1],b[1], b[3]);
    float D22 = mad(-L21D11, L21, SquaredDepthVariance);

    // Obtain a scaled inverse image of bz=(1,z[0],z[0]*z[0])^T
    vec3 c = vec3(1.0, z[0], z[0] * z[0]);
    // Forward substitution to solve L*c1=bz
    c[1] -= b.x;
    c[2] -= b.y + L21 * c[1];
    // Scaling to solve D*c2=c1
    c[1] *= InvD11;
    c[2] /= D22;
    // Backward substitution to solve L^T*c3=c2
    c[1] -= L21 * c[2];
    c[0] -= dot(c.yz, b.xy);
    // Solve the quadratic equation c[0]+c[1]*z+c[2]*z^2 to obtain solutions
    // z[1] and z[2]
    float InvC2 = 1.0 / c[2];
    float p = c[1] * InvC2;
    float q = c[0] * InvC2;
    float D = (p * p * 0.25) - q;
    float r = sqrt(D);
    z[1] =-p * 0.5 - r;
    z[2] =-p * 0.5 + r;
    // Compute the absorbance by summing the appropriate weights
    vec3 polynomial;
    vec3 weight_factor = vec3(overestimation, (z[1] < z[0]) ? 1.0 : 0.0, (z[2] < z[0]) ? 1.0 : 0.0);
    float f0 = weight_factor[0];
    float f1 = weight_factor[1];
    float f2 = weight_factor[2];
    float f01 = (f1 - f0) / (z[1] - z[0]);
    float f12 = (f2 - f1) / (z[2] - z[1]);
    float f012 = (f12 - f01) / (z[2] - z[0]);
    polynomial[0] = f012;
    polynomial[1] = polynomial[0];
    polynomial[0] = f01 - polynomial[0] * z[1];
    polynomial[2] = polynomial[1];
    polynomial[1] = polynomial[0] - polynomial[1] * z[0];
    polynomial[0] = f0-polynomial[0] * z[0];
    float absorbance = polynomial[0] + dot(b.xy, polynomial.yz);;
    // Turn the normalized absorbance into transmittance
    return clamp(exp(-b_0 * absorbance), 0.0, 1.0);
}

void ResolveMoments(float depth, float b0, vec4 b_1234, out float transmittance_at_depth, out float total_transmittance) {
    transmittance_at_depth = 1.0;
    total_transmittance = 1.0;

    if (b0 - 0.00100050033 < 0.0) discard;
    total_transmittance = exp(-b0);

    b_1234 /= b0;

    const vec4 bias_vector = vec4(0.0, 0.628, 0.0, 0.628);
    transmittance_at_depth = ComputeTransmittanceAtDepthFrom4PowerMoments(b0, b_1234, depth, 0.0035 /* moment_bias */, 0.1 /* overestimation */, bias_vector);
}

float TransparentDepthWeight(float z, float alpha) {
    //return alpha * clamp(0.1 * (1e-5 + 0.04 * z * z + pow6(0.005 * z)), 1e-2, 3e3);
    return alpha * max(3e3 * pow3(1.0 - z), 1e-2);
}
#endif

#line 9
#line 0
#ifndef _LTC_GLSL
#define _LTC_GLSL

///////////////////////////////
// Linearly Transformed Cosines
///////////////////////////////

const float LTC_LUT_SIZE  = 64.0;
const float LTC_LUT_SCALE = (LTC_LUT_SIZE - 1.0) / LTC_LUT_SIZE;
const float LTC_LUT_BIAS  = 0.5 / LTC_LUT_SIZE;

const float LTC_LUT_MIN_ROUGHNESS = 0.01;

//
// Rectangular light
//

#define CLIPLESS_APPROXIMATION 1

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2) {
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

float IntegrateEdge(vec3 v1, vec3 v2) {
    return IntegrateEdgeVec(v1, v2).z;
}

void ClipQuadToHorizon(inout vec3 L[5], out int n) {
    // detect clipping config
    int config = 0;
    if (L[0].z > 0.0) config += 1;
    if (L[1].z > 0.0) config += 2;
    if (L[2].z > 0.0) config += 4;
    if (L[3].z > 0.0) config += 8;

    // clip
    n = 0;

    if (config == 0) {
        // clip all
    } else if (config == 1) { // V1 clip V2 V3 V4
        n = 3;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[3].z * L[0] + L[0].z * L[3];
    } else if (config == 2) { // V2 clip V1 V3 V4
        n = 3;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    } else if (config == 3) { // V1 V2 clip V3 V4
        n = 4;
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
        L[3] = -L[3].z * L[0] + L[0].z * L[3];
    } else if (config == 4) { // V3 clip V1 V2 V4
        n = 3;
        L[0] = -L[3].z * L[2] + L[2].z * L[3];
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
    } else if (config == 5) { // V1 V3 clip V2 V4) impossible
        n = 0;
    } else if (config == 6) { // V2 V3 clip V1 V4
        n = 4;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    } else if (config == 7) { // V1 V2 V3 clip V4
        n = 5;
        L[4] = -L[3].z * L[0] + L[0].z * L[3];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    } else if (config == 8) { // V4 clip V1 V2 V3
        n = 3;
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
        L[1] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] =  L[3];
    } else if (config == 9) { // V1 V4 clip V2 V3
        n = 4;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[2].z * L[3] + L[3].z * L[2];
    } else if (config == 10) { // V2 V4 clip V1 V3) impossible
        n = 0;
    } else if (config == 11) { // V1 V2 V4 clip V3
        n = 5;
        L[4] = L[3];
        L[3] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    } else if (config == 12) { // V3 V4 clip V1 V2
        n = 4;
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
    } else if (config == 13) { // V1 V3 V4 clip V2
        n = 5;
        L[4] = L[3];
        L[3] = L[2];
        L[2] = -L[1].z * L[2] + L[2].z * L[1];
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
    } else if (config == 14) { // V2 V3 V4 clip V1
        n = 5;
        L[4] = -L[0].z * L[3] + L[3].z * L[0];
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
    } else if (config == 15) { // V1 V2 V3 V4
        n = 4;
    }

    if (n == 3) {
        L[3] = L[0];
    }
    if (n == 4) {
        L[4] = L[0];
    }
}

vec2 LTC_Coords(float cosTheta, float roughness) {
    float theta = sqrt(1.0 - saturate(cosTheta));
    vec2 coords = vec2(max(roughness, LTC_LUT_MIN_ROUGHNESS), theta);

    // scale and bias coordinates, for correct filtered lookup
    coords = coords * LTC_LUT_SCALE + LTC_LUT_BIAS;

    return coords;
}

vec3 LTC_Evaluate_Rect(sampler2D ltc_2, vec3 N, vec3 V, vec3 P, vec4 t1_fetch, vec3 points[4], bool two_sided) {
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    mat3 Minv = mat3(
        vec3(t1_fetch.x,       0.0, t1_fetch.y),
        vec3(      0.0,        1.0,        0.0),
        vec3(t1_fetch.z,       0.0, t1_fetch.w)
    );

    // rotate area light in (T1, T2, N) basis
    Minv = Minv * transpose(mat3(T1, T2, N));

    // polygon (allocate 5 vertices for clipping)
    vec3 L[5];
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    // integrate
    float sum = 0.0;

#if CLIPLESS_APPROXIMATION
    vec3 dir = points[0].xyz - P;
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    if (L[0].z < 0.0 && L[1].z < 0.0 && L[2].z < 0.0 && L[3].z < 0.0) {
        return vec3(0.0);
    }

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    vec3 vsum = vec3(0.0);

    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);
    float z = vsum.z / len;

    if (behind) {
        z = -z;
    }

    vec2 uv = vec2(z * 0.5 + 0.5, len);
    uv = uv * LTC_LUT_SCALE + LTC_LUT_BIAS;

    float scale = textureLod(ltc_2, uv, 0.0).w;

    sum = len * scale;

    if (behind && !two_sided) {
        sum = 0.0;
    }
#else
    int n;
    ClipQuadToHorizon(L, n);

    if (n == 0) {
        return vec3(0, 0, 0);
    }
    // project onto sphere
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);
    L[4] = normalize(L[4]);

    // integrate
    sum += IntegrateEdge(L[0], L[1]);
    sum += IntegrateEdge(L[1], L[2]);
    sum += IntegrateEdge(L[2], L[3]);
    if (n >= 4) {
        sum += IntegrateEdge(L[3], L[4]);
    }
    if (n == 5) {
        sum += IntegrateEdge(L[4], L[0]);
    }
    sum = two_sided ? abs(sum) : max(0.0, sum);
#endif

    return vec3(sum, sum, sum);
}

//
// Disk light
//

// An extended version of the implementation from
// "How to solve a cubic equation, revisited"
// http://momentsingraphics.de/?p=105
vec3 SolveCubic(vec4 Coefficient) {
    // Normalize the polynomial
    Coefficient.xyz /= Coefficient.w;
    // Divide middle coefficients by three
    Coefficient.yz /= 3.0;

    float A = Coefficient.w;
    float B = Coefficient.z;
    float C = Coefficient.y;
    float D = Coefficient.x;

    // Compute the Hessian and the discriminant
    vec3 Delta = vec3(
        -Coefficient.z * Coefficient.z + Coefficient.y,
        -Coefficient.y * Coefficient.z + Coefficient.x,
        dot(vec2(Coefficient.z, -Coefficient.y), Coefficient.xy)
    );

    float Discriminant = max(dot(vec2(4.0 * Delta.x, -Delta.y), Delta.zy), 0.0);

    vec3 RootsA, RootsD;

    vec2 xlc, xsc;

    { // Algorithm A
        float A_a = 1.0;
        float C_a = Delta.x;
        float D_a = -2.0 * B * Delta.x + Delta.y;

        // Take the cubic root of a normalized complex number
        float Theta = atan(sqrt(Discriminant), -D_a) / 3.0;

        float x_1a = 2.0 * sqrt(-C_a) * cos(Theta);
        float x_3a = 2.0 * sqrt(-C_a) * cos(Theta + (2.0 / 3.0) * M_PI);

        float xl;
        if ((x_1a + x_3a) > 2.0 * B) {
            xl = x_1a;
        } else {
            xl = x_3a;
        }
        xlc = vec2(xl - B, A);
    }

    { // Algorithm D
        float A_d = D;
        float C_d = Delta.z;
        float D_d = -D * Delta.y + 2.0 * C * Delta.z;

        // Take the cubic root of a normalized complex number
        float Theta = atan(D * sqrt(Discriminant), -D_d) / 3.0;

        float x_1d = 2.0 * sqrt(-C_d) * cos(Theta);
        float x_3d = 2.0 * sqrt(-C_d) * cos(Theta + (2.0 / 3.0) * M_PI);

        float xs;
        if (x_1d + x_3d < 2.0 * C) {
            xs = x_1d;
        } else {
            xs = x_3d;
        }
        xsc = vec2(-D, xs + C);
    }

    float E =  xlc.y * xsc.y;
    float F = -xlc.x * xsc.y - xlc.y * xsc.x;
    float G =  xlc.x * xsc.x;

    vec2 xmc = vec2(C * F - B * G, -B * F + C * E);

    vec3 Root = vec3(xsc.x / xsc.y, xmc.x / xmc.y, xlc.x / xlc.y);

    if (Root.x < Root.y && Root.x < Root.z) {
        Root.xyz = Root.yxz;
    } else if (Root.z < Root.x && Root.z < Root.y) {
        Root.xyz = Root.xzy;
    }
    return Root;
}

vec3 LTC_Evaluate_Disk(sampler2D ltc_2, vec3 N, vec3 V, vec3 P, vec4 t1_fetch, vec3 points[4], bool two_sided) {
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    mat3 R = transpose(mat3(T1, T2, N));

    // polygon (allocate 3 vertices for clipping)
    vec3 L_[3];
    L_[0] = R * (points[0] - P);
    L_[1] = R * (points[1] - P);
    L_[2] = R * (points[2] - P);

    if (L_[0].z < -0.25 && L_[1].z < -0.25 && L_[2].z < -0.25) {
        return vec3(0.0);
    }

    // init ellipse
    vec3 C  = 0.5 * (L_[0] + L_[2]);
    vec3 V1 = 0.5 * (L_[1] - L_[2]);
    vec3 V2 = 0.5 * (L_[1] - L_[0]);

    mat3 Minv = mat3(
        vec3(t1_fetch.x,       0.0, t1_fetch.y),
        vec3(      0.0,        1.0,        0.0),
        vec3(t1_fetch.z,       0.0, t1_fetch.w)
    );

    C  = Minv * C;
    V1 = Minv * V1;
    V2 = Minv * V2;

    if(!two_sided && dot(cross(V1, V2), C) < 0.0) {
        return vec3(0.0);
    }

    // compute eigenvectors of ellipse
    float a, b;
    float d11 = dot(V1, V1);
    float d22 = dot(V2, V2);
    float d12 = dot(V1, V2);
    if (abs(d12) / sqrt(d11 * d22) > 0.001) {
        float tr = d11 + d22;
        float det = max(-d12 * d12 + d11 * d22, 0.0);

        // use sqrt matrix to solve for eigenvalues
        det = sqrt(det);
        float u = 0.5 * sqrt(tr - 2.0 * det);
        float v = 0.5 * sqrt(tr + 2.0 * det);
        float e_max = (u + v) * (u + v);
        float e_min = (u - v) * (u - v);

        vec3 V1_, V2_;
        if (d11 > d22) {
            V1_ = d12 * V1 + (e_max - d11) * V2;
            V2_ = d12 * V1 + (e_min - d11) * V2;
        } else {
            V1_ = d12 * V2 + (e_max - d22) * V1;
            V2_ = d12 * V2 + (e_min - d22) * V1;
        }

        a = 1.0 / e_max;
        b = 1.0 / e_min;
        V1 = normalize(V1_);
        V2 = normalize(V2_);
    } else {
        a = 1.0 / dot(V1, V1);
        b = 1.0 / dot(V2, V2);
        V1 *= sqrt(a);
        V2 *= sqrt(b);
    }

    vec3 V3 = cross(V1, V2);
    if (dot(C, V3) < 0.0) {
        V3 *= -1.0;
    }

    float L  = dot(V3, C);
    float x0 = dot(V1, C) / L;
    float y0 = dot(V2, C) / L;

    float E1 = inversesqrt(a);
    float E2 = inversesqrt(b);

    a *= L * L;
    b *= L * L;

    float c0 = a * b;
    float c1 = a * b * (1.0 + x0 * x0 + y0 * y0) - a - b;
    float c2 = 1.0 - a * (1.0 + x0 * x0) - b * (1.0 + y0 * y0);
    float c3 = 1.0;

    vec3 roots = SolveCubic(vec4(c0, c1, c2, c3));
    float e1 = roots.x;
    float e2 = roots.y;
    float e3 = roots.z;

    vec3 avgDir = vec3(a * x0 / (a - e2), b * y0 / (b - e2), 1.0);

    mat3 rotate = mat3(V1, V2, V3);

    avgDir = rotate * avgDir;
    avgDir = normalize(avgDir);

    float L1 = sqrt(max(-e2 / e3, 0.0));
    float L2 = sqrt(max(-e2 / e1, 0.0));

    float formFactor = L1 * L2 * inversesqrt((1.0 + L1 * L1) * (1.0 + L2 * L2));

    // use tabulated horizon-clipped sphere
    vec2 uv = vec2(avgDir.z * 0.5 + 0.5, formFactor);
    uv = uv * LTC_LUT_SCALE + LTC_LUT_BIAS;

    float scale = textureLod(ltc_2, uv, 0.0).w;

    float spec = formFactor * scale;

    return vec3(spec, spec, spec);
}

//
// Line light
//

float Fpo(float d, float l) {
    return l / (d * (d * d + l * l)) + atan(l/d) / (d*d);
}

float Fwt(float d, float l) {
    return l * l / (d * (d * d + l * l));
}

float I_diffuse_line(vec3 p1, vec3 p2) {
    // tangent
    vec3 wt = normalize(p2 - p1);

    // clamping
    if (p1.z <= 0.0 && p2.z <= 0.0) {
        return 0.0;
    }
    if (p1.z < 0.0) {
        p1 = (+p1 * p2.z - p2 * p1.z) / (+p2.z - p1.z);
    }
    if (p2.z < 0.0) {
        p2 = (-p1 * p2.z + p2 * p1.z) / (-p2.z + p1.z);
    }

    // parameterization
    float l1 = dot(p1, wt);
    float l2 = dot(p2, wt);

    // shading point orthonormal projection on the line
    vec3 po = p1 - l1 * wt;

    // distance to line
    float d = length(po);

    // integral
    float I = (Fpo(d, l2) - Fpo(d, l1)) * po.z + (Fwt(d, l2) - Fwt(d, l1)) * wt.z;
    return I / M_PI;
}

float I_ltc_line(vec3 p1, vec3 p2, mat3 Minv) {
    // transform to diffuse configuration
    vec3 p1o = Minv * p1;
    vec3 p2o = Minv * p2;
    float I_diffuse = I_diffuse_line(p1o, p2o);

    // width factor
    vec3 ortho = normalize(cross(p1, p2));
    float w =  1.0 / length(inverse(transpose(Minv)) * ortho);

    return w * I_diffuse;
}

vec3 LTC_Evaluate_Line(vec3 N, vec3 V, vec3 P, vec4 t1_fetch, vec3 points[2], float radius) {
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    mat3 B = transpose(mat3(T1, T2, N));

    vec3 p1 = B * (points[0] - P);
    vec3 p2 = B * (points[1] - P);

    mat3 Minv = mat3(
        vec3(t1_fetch.x,       0.0, t1_fetch.y),
        vec3(      0.0,        1.0,        0.0),
        vec3(t1_fetch.z,       0.0, t1_fetch.w)
    );

    float Iline = radius * I_ltc_line(p1, p2, Minv);
    return vec3(clamp(Iline, 0.0, 1.0));
}

#endif // _LTC_GLSL

#line 10
#line 0
#ifndef _PRINCIPLED_GLSL
#define _PRINCIPLED_GLSL

float fresnel_dielectric_cos(float cosi, float eta) {
    // compute fresnel reflectance without explicitly computing the refracted direction
    float c = abs(cosi);
    float g = eta * eta - 1 + c * c;
    float result;

    if (g > 0) {
        g = sqrt(g);
        float A = (g - c) / (g + c);
        float B = (c * (g + c) - 1) / (c * (g - c) + 1);
        result = 0.5 * A * A * (1 + B * B);
    } else {
        result = 1.0; // TIR (no refracted component)
    }

    return result;
}

void get_lobe_weights(float base_color_lum, float spec_color_lum, float specular,
                      float metallic, float transmission, float clearcoat, out float out_diffuse_weight,
                      out float out_specular_weight, out float out_clearcoat_weight, out float out_refraction_weight) {
    // taken from Cycles
    out_diffuse_weight = base_color_lum * (1.0 - metallic) * (1.0 - transmission);
    float final_transmission = transmission * (1.0 - metallic);
    out_specular_weight =
        (specular != 0.0 || metallic != 0.0) ? spec_color_lum * (1.0 - final_transmission) : 0.0;
    out_clearcoat_weight = 0.25 * clearcoat * (1.0 - metallic);
    out_refraction_weight = final_transmission * base_color_lum;

    float total_weight =
        out_diffuse_weight + out_specular_weight + out_clearcoat_weight + out_refraction_weight;
    if (total_weight != 0.0) {
        out_diffuse_weight /= total_weight;
        out_specular_weight /= total_weight;
        out_clearcoat_weight /= total_weight;
        out_refraction_weight /= total_weight;
    }
}

#endif // _PRINCIPLED_GLSL

#line 11
#line 0
#ifndef GBUFFER_SHADE_INTERFACE_H
#define GBUFFER_SHADE_INTERFACE_H

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

INTERFACE_START(GBufferShade)

struct Params {
    UVEC2_TYPE img_size;
};

#define LOCAL_GROUP_SIZE_X 8
#define LOCAL_GROUP_SIZE_Y 8

#define DEPTH_TEX_SLOT 1
#define ALBEDO_TEX_SLOT 2
#define NORMAL_TEX_SLOT 3
#define SPECULAR_TEX_SLOT 4

#define SHADOW_TEX_SLOT 5
#define SSAO_TEX_SLOT 6
#define LIGHT_BUF_SLOT 7
#define DECAL_BUF_SLOT 8
#define CELLS_BUF_SLOT 9
#define ITEMS_BUF_SLOT 10
#define GI_TEX_SLOT 11
#define SUN_SHADOW_TEX_SLOT 12
#define LTC_DIFF_LUT_TEX_SLOT 13
#define LTC_SHEEN_LUT_TEX_SLOT 15
#define LTC_SPEC_LUT_TEX_SLOT 17
#define LTC_COAT_LUT_TEX_SLOT 19

#define OUT_COLOR_IMG_SLOT 0

INTERFACE_END

#endif // GBUFFER_SHADE_INTERFACE_H

#line 12

#define LIGHT_ATTEN_CUTOFF 0.004

#define ENABLE_SPHERE_LIGHT 1
#define ENABLE_RECT_LIGHT 1
#define ENABLE_DISK_LIGHT 1
#define ENABLE_LINE_LIGHT 1

#define ENABLE_DIFFUSE 1
#define ENABLE_SHEEN 1
#define ENABLE_SPECULAR 1
#define ENABLE_CLEARCOAT 1

/*
UNIFORM_BLOCKS
    SharedDataBlock : 23
    UniformParams : 24
PERM @HQ_HDR
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

layout(binding = ALBEDO_TEX_SLOT) uniform sampler2D g_albedo_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORMAL_TEX_SLOT) uniform sampler2D g_normal_tex;
layout(binding = SPECULAR_TEX_SLOT) uniform usampler2D g_specular_tex;

layout(binding = SHADOW_TEX_SLOT) uniform sampler2DShadow g_shadow_tex;
layout(binding = SSAO_TEX_SLOT) uniform sampler2D g_ao_tex;
layout(binding = LIGHT_BUF_SLOT) uniform highp samplerBuffer g_lights_buf;
layout(binding = DECAL_BUF_SLOT) uniform mediump samplerBuffer g_decals_buf;
layout(binding = CELLS_BUF_SLOT) uniform highp usamplerBuffer g_cells_buf;
layout(binding = ITEMS_BUF_SLOT) uniform highp usamplerBuffer g_items_buf;
layout(binding = GI_TEX_SLOT) uniform sampler2D g_gi_tex;
layout(binding = SUN_SHADOW_TEX_SLOT) uniform sampler2D g_sun_shadow_tex;
layout(binding = LTC_DIFF_LUT_TEX_SLOT) uniform sampler2D g_ltc_diff_lut[2];
layout(binding = LTC_SHEEN_LUT_TEX_SLOT) uniform sampler2D g_ltc_sheen_lut[2];
layout(binding = LTC_SPEC_LUT_TEX_SLOT) uniform sampler2D g_ltc_spec_lut[2];
layout(binding = LTC_COAT_LUT_TEX_SLOT) uniform sampler2D g_ltc_coat_lut[2];

#ifdef HQ_HDR
layout(binding = OUT_COLOR_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;
#else
layout(binding = OUT_COLOR_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_color_img;
#endif

vec3 EvaluateLightSource() {
    return vec3(0.0);
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 norm_uvs = (vec2(icoord) + 0.5) / g_shrd_data.res_and_fres.xy;

    highp float depth = texelFetch(g_depth_tex, icoord, 0).r;
    highp float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);
    highp float k = log2(lin_depth / g_shrd_data.clip_info[1]) / g_shrd_data.clip_info[3];
    int slice = clamp(int(k * float(REN_GRID_RES_Z)), 0, REN_GRID_RES_Z - 1);

    int ix = int(gl_GlobalInvocationID.x), iy = int(gl_GlobalInvocationID.y);
    int cell_index = GetCellIndex(ix, iy, slice, g_shrd_data.res_and_fres.xy);

    highp uvec2 cell_data = texelFetch(g_cells_buf, cell_index).xy;
    highp uvec2 offset_and_lcount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.x, 24, 8));
    highp uvec2 dcount_and_pcount = uvec2(bitfieldExtract(cell_data.y, 0, 8), bitfieldExtract(cell_data.y, 8, 8));

    vec4 pos_cs = vec4(norm_uvs, depth, 1.0);
#if defined(VULKAN)
    pos_cs.xy = 2.0 * pos_cs.xy - 1.0;
    pos_cs.y = -pos_cs.y;
#else // VULKAN
    pos_cs.xyz = 2.0 * pos_cs.xyz - 1.0;
#endif // VULKAN

    vec4 pos_ws = g_shrd_data.inv_view_proj_no_translation * pos_cs;
    pos_ws /= pos_ws.w;
    pos_ws.xyz += g_shrd_data.cam_pos_and_gamma.xyz;

    vec3 base_color = texelFetch(g_albedo_tex, icoord, 0).rgb;
    vec4 normal = UnpackNormalAndRoughness(texelFetch(g_normal_tex, icoord, 0));
    uint packed_mat_params = texelFetch(g_specular_tex, icoord, 0).r;

    //
    // Artificial lights
    //
    vec3 additional_light = vec3(0.0);

    vec3 V = normalize(g_shrd_data.cam_pos_and_gamma.xyz - pos_ws.xyz);
    float N_dot_V = saturate(dot(normal.xyz, V));

    vec3 I = V;
    vec3 N = normal.xyz;

    vec3 tint_color = vec3(0.0);

    const float base_color_lum = lum(base_color);
    if (base_color_lum > 0.0) {
        tint_color = base_color / base_color_lum;
    }

    vec4 mat_params0, mat_params1;
    UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);

    float roughness = normal.w;
    float sheen = mat_params0.x;
    float sheen_tint = mat_params0.y;
    float specular = mat_params0.z;
    float specular_tint = mat_params0.w;
    float metallic = mat_params1.x;
    float transmission = mat_params1.y;
    float clearcoat = mat_params1.z;
    float clearcoat_roughness = mat_params1.w;

    vec3 spec_tmp_col = mix(vec3(1.0), tint_color, specular_tint);
    spec_tmp_col = mix(specular * 0.08 * spec_tmp_col, base_color, metallic);

    float spec_ior = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
    float spec_F0 = fresnel_dielectric_cos(1.0, spec_ior);

    // Approximation of FH (using shading normal)
    float FN = (fresnel_dielectric_cos(dot(I, N), spec_ior) - spec_F0) / (1.0 - spec_F0);

    vec3 approx_spec_col = mix(spec_tmp_col, vec3(1.0), FN * (1.0 - roughness));
    float spec_color_lum = lum(approx_spec_col);

    float diffuse_weight, specular_weight, clearcoat_weight, refraction_weight;
    get_lobe_weights(mix(base_color_lum, 1.0, sheen), spec_color_lum, specular, metallic, transmission, clearcoat,
                     diffuse_weight, specular_weight, clearcoat_weight, refraction_weight);

    vec3 sheen_color = sheen * mix(vec3(1.0), tint_color, sheen_tint);

    float clearcoat_ior = (2.0 / (1.0 - sqrt(0.08 * clearcoat))) - 1.0;
    float clearcoat_F0 = fresnel_dielectric_cos(1.0, clearcoat_ior);
    float clearcoat_roughness2 = clearcoat_roughness * clearcoat_roughness;

    // Approximation of FH (using shading normal)
    float clearcoat_FN = (fresnel_dielectric_cos(dot(I, N), clearcoat_ior) - clearcoat_F0) / (1.0 - clearcoat_F0);

    vec3 approx_clearcoat_col = vec3(mix(/*clearcoat * 0.08*/ 0.04, 1.0, clearcoat_FN));

    //
    // Fetch LTC data
    //

    vec2 ltc_uv = LTC_Coords(N_dot_V, roughness);

    vec4 diff_t1 = textureLod(g_ltc_diff_lut[0], ltc_uv, 0.0);
    vec2 diff_t2 = textureLod(g_ltc_diff_lut[1], ltc_uv, 0.0).xy;

    vec4 sheen_t1 = textureLod(g_ltc_sheen_lut[0], ltc_uv, 0.0);
    vec2 sheen_t2 = textureLod(g_ltc_sheen_lut[1], ltc_uv, 0.0).xy;

    vec4 spec_t1 = textureLod(g_ltc_spec_lut[0], ltc_uv, 0.0);
    vec2 spec_t2 = textureLod(g_ltc_spec_lut[1], ltc_uv, 0.0).xy;

    vec2 coat_ltc_uv = LTC_Coords(N_dot_V, clearcoat_roughness2);
    vec4 coat_t1 = textureLod(g_ltc_coat_lut[0], coat_ltc_uv, 0.0);
    vec2 coat_t2 = textureLod(g_ltc_coat_lut[1], coat_ltc_uv, 0.0).xy;

    //
    // Evaluate lights
    //

    vec2 debug_uvs = vec2(0.0), _unused;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + offset_and_lcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int li = int(bitfieldExtract(item_data, 0, 12));

        vec4 col_and_type = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 0);
        vec4 pos_and_radius = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 1);
        vec4 dir_and_spot = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 2);

        const bool TwoSided = false;

        int type = floatBitsToInt(col_and_type.w);
        if (type == LIGHT_TYPE_SPHERE && ENABLE_SPHERE_LIGHT != 0) {
            vec3 lp = pos_and_radius.xyz;

            // TODO: simplify this!
            vec3 to = normalize(pos_ws.xyz - lp);
            vec3 u = normalize(V - to * dot(V, to));
            vec3 v = cross(to, u);

            u *= pos_and_radius.w;
            v *= pos_and_radius.w;

            vec3 points[4];
            points[0] = lp + u + v;
            points[1] = lp + u - v;
            points[2] = lp - u - v;
            points[3] = lp - u + v;

            if (diffuse_weight > 0.0 && ENABLE_DIFFUSE != 0) {
                vec3 dcol = base_color;

                vec3 diff = LTC_Evaluate_Disk(g_ltc_diff_lut[1], N, V, pos_ws.xyz, diff_t1, points, TwoSided);
                diff *= dcol * diff_t2.x;// + (1.0 - dcol) * diff_t2.y;

                if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                    vec3 _sheen = LTC_Evaluate_Disk(g_ltc_sheen_lut[1], N, V, pos_ws.xyz, sheen_t1, points, TwoSided);
                    diff += _sheen * (sheen_color * sheen_t2.x + (1.0 - sheen_color) * sheen_t2.y);
                }

                additional_light += col_and_type.xyz * diff / M_PI;
            }

            if (specular_weight > 0.0 && ENABLE_SPECULAR != 0) {
                vec3 scol = approx_spec_col;

                vec3 spec = LTC_Evaluate_Disk(g_ltc_spec_lut[1], N, V, pos_ws.xyz, spec_t1, points, TwoSided);
                spec *= scol * spec_t2.x + (1.0 - scol) * spec_t2.y;

                additional_light += col_and_type.xyz * spec / M_PI;
            }

            if (clearcoat_weight > 0.0 && ENABLE_CLEARCOAT != 0) {
                vec3 ccol = approx_clearcoat_col;

                vec3 coat = LTC_Evaluate_Disk(g_ltc_coat_lut[1], N, V, pos_ws.xyz, coat_t1, points, TwoSided);
                coat *= ccol * coat_t2.x + (1.0 - ccol) * coat_t2.y;

                additional_light += 0.25 * col_and_type.xyz * coat / M_PI;
            }
        } else if (type == LIGHT_TYPE_RECT && ENABLE_RECT_LIGHT != 0) {
            vec3 lp = pos_and_radius.xyz;

            vec3 u = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 3).xyz;
            vec3 v = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 4).xyz;

            vec3 points[4];
            points[0] = lp + u + v;
            points[1] = lp + u - v;
            points[2] = lp - u - v;
            points[3] = lp - u + v;

            if (diffuse_weight > 0.0 && ENABLE_DIFFUSE != 0) {
                vec3 dcol = base_color;

                vec3 diff = LTC_Evaluate_Rect(g_ltc_diff_lut[1], N, V, pos_ws.xyz, diff_t1, points, TwoSided);
                diff *= dcol * diff_t2.x;// + (1.0 - dcol) * diff_t2.y;

                if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                    vec3 _sheen = LTC_Evaluate_Rect(g_ltc_sheen_lut[1], N, V, pos_ws.xyz, sheen_t1, points, TwoSided);
                    diff += _sheen * (sheen_color * sheen_t2.x + (1.0 - sheen_color) * sheen_t2.y);
                }

                additional_light += col_and_type.xyz * diff / 4.0;
            }

            if (specular_weight > 0.0 && ENABLE_SPECULAR != 0) {
                vec3 scol = approx_spec_col;

                vec3 spec = LTC_Evaluate_Rect(g_ltc_spec_lut[1], N, V, pos_ws.xyz, spec_t1, points, TwoSided);
                spec *= scol * spec_t2.x + (1.0 - scol) * spec_t2.y;

                additional_light += col_and_type.xyz * spec / 4.0;
            }

            if (clearcoat_weight > 0.0 && ENABLE_CLEARCOAT != 0) {
                vec3 ccol = approx_clearcoat_col;

                vec3 coat = LTC_Evaluate_Rect(g_ltc_coat_lut[1], N, V, pos_ws.xyz, coat_t1, points, TwoSided);
                coat *= ccol * coat_t2.x + (1.0 - ccol) * coat_t2.y;

                additional_light += 0.25 * col_and_type.xyz * coat / 4.0;
            }
        } else if (type == LIGHT_TYPE_DISK && ENABLE_DISK_LIGHT != 0) {
            vec3 lp = pos_and_radius.xyz;

            vec3 u = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 3).xyz;
            vec3 v = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 4).xyz;

            vec3 points[4];
            points[0] = lp + u + v;
            points[1] = lp + u - v;
            points[2] = lp - u - v;
            points[3] = lp - u + v;

            if (diffuse_weight > 0.0 && ENABLE_DIFFUSE != 0) {
                vec3 dcol = base_color;

                vec3 diff = LTC_Evaluate_Disk(g_ltc_diff_lut[1], N, V, pos_ws.xyz, diff_t1, points, TwoSided);
                diff *= dcol * diff_t2.x;// + (1.0 - dcol) * diff_t2.y;

                if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                    vec3 _sheen = LTC_Evaluate_Disk(g_ltc_sheen_lut[1], N, V, pos_ws.xyz, sheen_t1, points, TwoSided);
                    diff += _sheen * (sheen_color * sheen_t2.x + (1.0 - sheen_color) * sheen_t2.y);
                }

                additional_light += col_and_type.xyz * diff / 4.0;
            }

            if (specular_weight > 0.0 && ENABLE_SPECULAR != 0) {
                vec3 scol = approx_spec_col;

                vec3 spec = LTC_Evaluate_Disk(g_ltc_spec_lut[1], N, V, pos_ws.xyz, spec_t1, points, TwoSided);
                spec *= scol * spec_t2.x + (1.0 - scol) * spec_t2.y;

                additional_light += col_and_type.xyz * spec / 4.0;
            }

            if (clearcoat_weight > 0.0 && ENABLE_CLEARCOAT != 0) {
                vec3 ccol = approx_clearcoat_col;

                vec3 coat = LTC_Evaluate_Disk(g_ltc_coat_lut[1], N, V, pos_ws.xyz, coat_t1, points, TwoSided);
                coat *= ccol * coat_t2.x + (1.0 - ccol) * coat_t2.y;

                additional_light += 0.25 * col_and_type.xyz * coat / 4.0;
            }
        } else if (type == LIGHT_TYPE_LINE && ENABLE_LINE_LIGHT != 0) {
            vec3 lp = pos_and_radius.xyz;

            vec3 u = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 3).xyz;
            vec3 v = texelFetch(g_lights_buf, li * LIGHTS_BUF_STRIDE + 4).xyz;

            vec3 points[2];
            points[0] = lp + v;
            points[1] = lp - v;

            if (diffuse_weight > 0.0 && ENABLE_DIFFUSE != 0) {
                vec3 dcol = base_color;

                vec3 diff = LTC_Evaluate_Line(N, V, pos_ws.xyz, diff_t1, points, 0.01);
                diff *= dcol * diff_t2.x;// + (1.0 - dcol) * diff_t2.y;

                if (sheen > 0.0 && ENABLE_SHEEN != 0) {
                    vec3 _sheen = LTC_Evaluate_Line(N, V, pos_ws.xyz, sheen_t1, points, 0.01);
                    diff += _sheen * (sheen_color * sheen_t2.x + (1.0 - sheen_color) * sheen_t2.y);
                }

                additional_light += col_and_type.xyz * diff;
            }

            if (specular_weight > 0.0 && ENABLE_SPECULAR != 0) {
                vec3 scol = approx_spec_col;

                vec3 spec = LTC_Evaluate_Line(N, V, pos_ws.xyz, spec_t1, points, 0.01);
                spec *= scol * spec_t2.x + (1.0 - scol) * spec_t2.y;

                additional_light += col_and_type.xyz * spec;
            }

            if (clearcoat_weight > 0.0 && ENABLE_CLEARCOAT != 0) {
                vec3 ccol = approx_clearcoat_col;

                vec3 coat = LTC_Evaluate_Line(N, V, pos_ws.xyz, coat_t1, points, 0.01);
                coat *= ccol * coat_t2.x + (1.0 - ccol) * coat_t2.y;

                additional_light += 0.25 * col_and_type.xyz * coat / 4.0;
            }
        }
    }

    //
    // Indirect probes
    //
    vec3 indirect_col = vec3(0.0);
    float total_fade = 0.0;

    for (uint i = offset_and_lcount.x; i < offset_and_lcount.x + dcount_and_pcount.y; i++) {
        highp uint item_data = texelFetch(g_items_buf, int(i)).x;
        int pi = int(bitfieldExtract(item_data, 24, 8));

        float dist = distance(g_shrd_data.probes[pi].pos_and_radius.xyz, pos_ws.xyz);
        float fade = 1.0 - smoothstep(0.9, 1.0, dist / g_shrd_data.probes[pi].pos_and_radius.w);

        indirect_col += fade * EvalSHIrradiance_NonLinear(normal.xyz, g_shrd_data.probes[pi].sh_coeffs[0],
                                                                      g_shrd_data.probes[pi].sh_coeffs[1],
                                                                      g_shrd_data.probes[pi].sh_coeffs[2]);
        total_fade += fade;
    }

    indirect_col /= max(total_fade, 1.0);
    indirect_col = max(1.0 * indirect_col, vec3(0.0));

    vec2 px_uvs = (vec2(ix, iy) + 0.5) / g_shrd_data.res_and_fres.zw;

    float lambert = clamp(dot(normal.xyz, g_shrd_data.sun_dir.xyz), 0.0, 1.0);
    float visibility = 0.0;
#if 0
    if (lambert > 0.00001) {
        vec4 g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2;

        const vec2 offsets[4] = vec2[4](
            vec2(0.0, 0.0),
            vec2(0.25, 0.0),
            vec2(0.0, 0.5),
            vec2(0.25, 0.5)
        );

        /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
            vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[i].clip_from_world * pos_ws).xyz;
    #if defined(VULKAN)
            shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;
    #else // VULKAN
            shadow_uvs = 0.5 * shadow_uvs + 0.5;
    #endif // VULKAN
            shadow_uvs.xy *= vec2(0.25, 0.5);
            shadow_uvs.xy += offsets[i];
    #if defined(VULKAN)
            shadow_uvs.y = 1.0 - shadow_uvs.y;
    #endif // VULKAN
            g_vtx_sh_uvs0[i] = shadow_uvs[0];
            g_vtx_sh_uvs1[i] = shadow_uvs[1];
            g_vtx_sh_uvs2[i] = shadow_uvs[2];
        }

        visibility = GetSunVisibility(lin_depth, g_shadow_tex, transpose(mat3x4(g_vtx_sh_uvs0, g_vtx_sh_uvs1, g_vtx_sh_uvs2)));
    }
#else
    if (lambert > 0.00001) {
        visibility = texelFetch(g_sun_shadow_tex, ivec2(ix, iy), 0).r;
    }
#endif

    vec4 gi = textureLod(g_gi_tex, px_uvs, 0.0);
    //vec3 gi = vec3(textureLod(g_gi_tex, px_uvs, 0.0).a * 0.01);
    //float ao = (gi.a * 0.01);
    vec3 diffuse_color = base_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
                                       gi.rgb /*+ ao * indirect_col +*/) + additional_light;

    //float ambient_occlusion = textureLod(g_ao_tex, px_uvs, 0.0).r;
    //vec3 diffuse_color = base_color * (g_shrd_data.sun_col.xyz * lambert * visibility +
    //                                   ambient_occlusion * ambient_occlusion * indirect_col +
    //                                   additional_light);

    imageStore(g_out_color_img, icoord, vec4(diffuse_color, 0.0));
    //imageStore(g_out_color_img, icoord, vec4(N * 0.5 + vec3(0.5), 0.0));
}
