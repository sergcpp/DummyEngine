#version 430

#if defined(GL_ES) || defined(VULKAN)
    precision mediump int;
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
#ifndef TAA_COMMON_GLSL
#define TAA_COMMON_GLSL

// http://twvideo01.ubm-us.net/o1/vault/gdc2016/Presentations/Pedersen_LasseJonFuglsang_TemporalReprojectionAntiAliasing.pdf
vec3 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec3 q) {
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.0001;

    vec3 v_clip = q - p_clip;
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max3(a_unit.x, a_unit.y, a_unit.z);

    if (ma_unit > 1.0) {
        return p_clip + v_clip / ma_unit;
    } else {
        return q; // point inside aabb
    }
}

// https://software.intel.com/en-us/node/503873
vec3 RGB_to_YCoCg(vec3 c) {
    // Y = R/4 + G/2 + B/4
    // Co = R/2 - B/2
    // Cg = -R/4 + G/2 - B/4
    return vec3(
         c.x/4.0 + c.y/2.0 + c.z/4.0,
         c.x/2.0 - c.z/2.0,
        -c.x/4.0 + c.y/2.0 - c.z/4.0
    );
}

vec3 YCoCg_to_RGB(vec3 c) {
    // R = Y + Co - Cg
    // G = Y + Cg
    // B = Y - Co - Cg
    return clamp(vec3(
        c.x + c.y - c.z,
        c.x + c.z,
        c.x - c.y - c.z
    ), vec3(0.0), vec3(1.0));
}

#endif // TAA_COMMON_GLSL

#line 10
#line 0
#ifndef BLIT_TAA_INTERFACE_H
#define BLIT_TAA_INTERFACE_H

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

INTERFACE_START(TempAA)

struct Params {
    VEC4_TYPE transform;
    VEC2_TYPE tex_size;
    float tonemap;
    float gamma;
    float exposure;
    float fade;
    float mix_factor; // for static accumulation
};

#define CURR_TEX_SLOT 0
#define HIST_TEX_SLOT 1
#define DEPTH_TEX_SLOT 2
#define VELOCITY_TEX_SLOT 3

INTERFACE_END

#endif // BLIT_TAA_INTERFACE_H

#line 11

/*
UNIFORM_BLOCKS
    UniformParams : 24
PERM @USE_CLIPPING
PERM @USE_ROUNDED_NEIBOURHOOD
PERM @USE_TONEMAP
PERM @USE_YCoCg
PERM @USE_CLIPPING;USE_TONEMAP
PERM @USE_CLIPPING;USE_TONEMAP;USE_YCoCg
PERM @USE_CLIPPING;USE_ROUNDED_NEIBOURHOOD
PERM @USE_CLIPPING;USE_ROUNDED_NEIBOURHOOD;USE_TONEMAP;USE_YCoCg
PERM @USE_STATIC_ACCUMULATION
*/

layout(binding = CURR_TEX_SLOT) uniform mediump sampler2D g_color_curr;
layout(binding = HIST_TEX_SLOT) uniform mediump sampler2D g_color_hist;

layout(binding = DEPTH_TEX_SLOT) uniform mediump sampler2D g_depth;
layout(binding = VELOCITY_TEX_SLOT) uniform mediump sampler2D g_velocity;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in highp vec2 g_vtx_uvs;

layout(location = 0) out vec3 g_out_color;
layout(location = 1) out vec3 g_out_history;

// https://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
vec3 Tonemap(in vec3 c) {
#if defined(USE_TONEMAP)
    c *= g_params.exposure;
    return c * rcp(max3(c.r, c.g, c.b) + 1.0);
#else
    return c;
#endif
}

vec3 TonemapInvert(in vec3 c) {
#if defined(USE_TONEMAP)
    return (c / max(g_params.exposure, 0.001)) * rcp(1.0 - max3(c.r, c.g, c.b));
#else
    return c;
#endif
}

vec3 FetchColor(sampler2D s, ivec2 icoord) {
#if defined(USE_YCoCg)
    return RGB_to_YCoCg(Tonemap(texelFetch(s, icoord, 0).rgb));
#else
    return Tonemap(texelFetch(s, icoord, 0).rgb);
#endif
}

vec3 SampleColor(sampler2D s, vec2 uvs) {
#if defined(USE_YCoCg)
    return RGB_to_YCoCg(Tonemap(textureLod(s, uvs, 0.0).rgb));
#else
    return Tonemap(textureLod(s, uvs, 0.0).rgb);
#endif
}

float Luma(vec3 col) {
#if defined(USE_YCoCg)
    return col.r;
#else
    return dot(col, vec3(0.2125, 0.7154, 0.0721));
#endif
}

vec3 find_closest_fragment_3x3(sampler2D dtex, vec2 uv, vec2 texel_size) {
    vec2 du = vec2(texel_size.x, 0.0);
    vec2 dv = vec2(0.0, texel_size.y);

    vec3 dtl = vec3(-1, -1, textureLod(dtex, uv - dv - du, 0.0).x);
    vec3 dtc = vec3( 0, -1, textureLod(dtex, uv - dv, 0.0).x);
    vec3 dtr = vec3( 1, -1, textureLod(dtex, uv - dv + du, 0.0).x);

    vec3 dml = vec3(-1, 0, textureLod(dtex, uv - du, 0.0).x);
    vec3 dmc = vec3( 0, 0, textureLod(dtex, uv, 0.0).x);
    vec3 dmr = vec3( 1, 0, textureLod(dtex, uv + du, 0.0).x);

    vec3 dbl = vec3(-1, 1, textureLod(dtex, uv + dv - du, 0.0).x);
    vec3 dbc = vec3( 0, 1, textureLod(dtex, uv + dv, 0.0).x);
    vec3 dbr = vec3( 1, 1, textureLod(dtex, uv + dv + du, 0.0).x);

    vec3 dmin = dtl;
    if (dmin.z > dtc.z) dmin = dtc;
    if (dmin.z > dtr.z) dmin = dtr;

    if (dmin.z > dml.z) dmin = dml;
    if (dmin.z > dmc.z) dmin = dmc;
    if (dmin.z > dmr.z) dmin = dmr;

    if (dmin.z > dbl.z) dmin = dbl;
    if (dmin.z > dbc.z) dmin = dbc;
    if (dmin.z > dbr.z) dmin = dbr;

    return vec3(uv + texel_size * dmin.xy, dmin.z);
}

void main() {
    ivec2 uvs_px = ivec2(g_vtx_uvs);
    vec2 texel_size = vec2(1.0) / g_params.tex_size;
    vec2 norm_uvs = g_vtx_uvs / g_params.tex_size;

    vec3 col_curr = FetchColor(g_color_curr, uvs_px);

#if defined(USE_STATIC_ACCUMULATION)
    vec3 col_hist = FetchColor(g_color_hist, uvs_px);
    vec3 col = mix(col_hist, col_curr, g_params.mix_factor);

    g_out_color = TonemapInvert(col);
    g_out_history = g_out_color;
#else // USE_STATIC_ACCUMULATION
    float min_depth = texelFetch(g_depth, uvs_px, 0).r;

    const ivec2 offsets[8] = ivec2[8](
        ivec2(-1, 1),   ivec2(0, 1),    ivec2(1, 1),
        ivec2(-1, 0),                   ivec2(1, 0),
        ivec2(-1, -1),  ivec2(1, -1),   ivec2(1, -1)
    );

#if !defined(USE_YCoCg) && !defined(USE_ROUNDED_NEIBOURHOOD)
    vec3 col_avg = col_curr, col_var = col_curr * col_curr;
    ivec2 closest_frag = ivec2(0, 0);

    for (int i = 0; i < 8; i++) {
        float depth = texelFetch(g_depth, uvs_px + offsets[i], 0).r;
        if (depth < min_depth) {
            closest_frag = offsets[i];
            min_depth = depth;
        }

        vec3 col = FetchColor(g_color_curr, uvs_px + offsets[i]);
        col_avg += col;
        col_var += col * col;
    }

    col_avg /= 9.0;
    col_var /= 9.0;

    vec3 sigma = sqrt(max(col_var - col_avg * col_avg, vec3(0.0)));
    vec3 col_min = col_avg - 1.25 * sigma;
    vec3 col_max = col_avg + 1.25 * sigma;

    vec2 closest_vel = texelFetch(g_velocity, clamp(uvs_px + closest_frag, ivec2(0), ivec2(g_params.tex_size - vec2(1))), 0).rg;
#else
    vec3 col_tl = SampleColor(g_color_curr, norm_uvs + vec2(-texel_size.x, -texel_size.y));
    vec3 col_tc = SampleColor(g_color_curr, norm_uvs + vec2(0, -texel_size.y));
    vec3 col_tr = SampleColor(g_color_curr, norm_uvs + vec2(texel_size.x, -texel_size.y));
    vec3 col_ml = SampleColor(g_color_curr, norm_uvs + vec2(-texel_size.x, 0));
    vec3 col_mc = col_curr;
    vec3 col_mr = SampleColor(g_color_curr, norm_uvs + vec2(texel_size.x, 0));
    vec3 col_bl = SampleColor(g_color_curr, norm_uvs + vec2(-texel_size.x, texel_size.y));
    vec3 col_bc = SampleColor(g_color_curr, norm_uvs + vec2(0, texel_size.y));
    vec3 col_br = SampleColor(g_color_curr, norm_uvs + vec2(texel_size.x, texel_size.y));

    vec3 col_min = min3(min3(col_tl, col_tc, col_tr),
                        min3(col_ml, col_mc, col_mr),
                        min3(col_bl, col_bc, col_br));
    vec3 col_max = max3(max3(col_tl, col_tc, col_tr),
                        max3(col_ml, col_mc, col_mr),
                        max3(col_bl, col_bc, col_br));

    vec3 col_avg = (col_tl + col_tc + col_tr + col_ml + col_mc + col_mr + col_bl + col_bc + col_br) / 9.0;

    #if defined(USE_ROUNDED_NEIBOURHOOD)
        vec3 col_min5 = min(col_tc, min(col_ml, min(col_mc, min(col_mr, col_bc))));
        vec3 col_max5 = max(col_tc, max(col_ml, max(col_mc, max(col_mr, col_bc))));
        vec3 col_avg5 = (col_tc + col_ml + col_mc + col_mr + col_bc) / 5.0;
        col_min = 0.5 * (col_min + col_min5);
        col_max = 0.5 * (col_max + col_max5);
        col_avg = 0.5 * (col_avg + col_avg5);
    #endif

    #if defined(USE_YCoCg)
        vec2 chroma_extent = vec2(0.25 * 0.5 * (col_max.r - col_min.r));
        vec2 chroma_center = col_curr.gb;
        col_min.yz = chroma_center - chroma_extent;
        col_max.yz = chroma_center + chroma_extent;
        col_avg.yz = chroma_center;
    #endif

    vec3 closest_frag = find_closest_fragment_3x3(g_depth, norm_uvs, texel_size);
    vec2 closest_vel = textureLod(g_velocity, closest_frag.xy, 0.0).rg;
#endif

    vec3 col_hist = SampleColor(g_color_hist, norm_uvs - closest_vel);

#if defined(USE_CLIPPING)
    col_hist = clip_aabb(col_min, col_max, col_hist);
#else
    col_hist = clamp(col_hist, col_min, col_max);
#endif

    const float HistoryWeightMin = 0.88;
    const float HistoryWeightMax = 0.97;

    float lum_curr = Luma(col_curr);
    float lum_hist = Luma(col_hist);

    float unbiased_diff = abs(lum_curr - lum_hist) / max3(lum_curr, lum_hist, 0.2);
    float unbiased_weight = 1.0 - unbiased_diff;
    float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
    float history_weight = mix(HistoryWeightMin, HistoryWeightMax, unbiased_weight_sqr);

    vec3 col = mix(col_curr, col_hist, history_weight);
#if defined(USE_YCoCg)
    col = YCoCg_to_RGB(col);
#endif
    g_out_color = TonemapInvert(col);
    g_out_history = g_out_color;
#endif // USE_STATIC_ACCUMULATION
}
