
#include "../../../src/Eng/Renderer/Renderer_GL_Defines.inl"

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

vec4 PackNormalAndRoughness(vec3 N, float roughness) {
    vec4 p;

    p.xyz = N;

    // Best fit
    float m = max(abs(N.x), max(abs(N.y), abs(N.z)));
    p.xyz *= positive_rcp(m);

    p.xyz = p.xyz * 0.5 + 0.5;
    p.w = roughness;

    return p;
}

vec4 UnpackNormalAndRoughness(vec4 p) {
    vec4 r;
    p.xyz = p.xyz * 2.0 - 1.0;
    r.xyz = p.xyz;
    r.w = p.w;

    r.xyz = normalize(r.xyz);

    return r;
}

// Ray Tracing Gems II, Listing 49-1
vec3 ReconstructViewPosition(vec2 uv, vec4 cam_frustum, float view_z, float is_ortho) {
    vec3 p;
    p.xy = uv.xy * cam_frustum.zw + cam_frustum.xy;

    p.xy *= view_z * (1.0 - abs(is_ortho)) + is_ortho;
    p.z = view_z;

    return p;
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
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[REN_MAX_SHADOWMAPS_TOTAL];
    vec4 uSunDir, uSunCol, uTaaInfo, uFrustumInfo;
    vec4 uClipInfo, uCamPosAndGamma, uCamDelta;
    vec4 uResAndFRes, uTranspParamsAndTime;
    vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[REN_MAX_PROBES_TOTAL];
    EllipsItem uEllipsoids[REN_MAX_ELLIPSES_TOTAL];
};

struct MaterialData {
    uint texture_indices[5];
    uint _pad[3];
    vec4 params;
};

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
}

#if defined(VULKAN) || defined(GL_SPIRV)
#define LAYOUT(x) layout(x)
#else
#define LAYOUT(x)
#endif