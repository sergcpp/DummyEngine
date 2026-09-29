#ifndef DOF_COMMON_GLSL
#define DOF_COMMON_GLSL

// length(vec2(0.5))
const float DOF_SINGLE_PIXEL = 0.7071;
const float NUM_DOF_RINGS = 3.0;
// In halfres pixels
const float EARLY_OUT_MAX_RADIUS = 2.0;

#define FOCAL_DEPTH g_params.focal_params.x
#define COC_FACTOR g_params.focal_params.y
#define FOCAL_LENGTH g_params.focal_params.z
#define MAX_KERNEL_RADIUS g_params.focal_params.w

// According to
// http://http.developer.nvidia.com/GPUGems/gpugems_ch23.html
// http://www.cambridgeincolour.com/tutorials/camera-lenses.htm
float blurRadiusFromDelta(float objectdistance, float focalDepth, float cocFactor, float focalLength) {
    return abs(cocFactor * (objectdistance - focalDepth) / (objectdistance * (focalDepth - focalLength)));
}

float SampleAlpha(float sampleCoc) {
    return min(1.0 / (M_PI * sampleCoc * sampleCoc), 1.0 / (M_PI * DOF_SINGLE_PIXEL * DOF_SINGLE_PIXEL));
}

// The samples are according to the slides by Jimenez:
// 1 Sample Center
// 8 Samples Ring 1
// 16 Samples Ring 2
// 24 Samples Ring 3
const vec3 dofSamplingRings[49] = vec3[](
    vec3(0, 0, 0),
    vec3(0, 0.333333, 1),
    vec3(0.235702, 0.235702, 1),
    vec3(0.333333, 4.2253e-07, 1),
    vec3(0.235702, -0.235702, 1),
    vec3(8.4506e-07, -0.333333, 1),
    vec3(-0.235701, -0.235703, 1),
    vec3(-0.333333, -1.26759e-06, 1),
    vec3(-0.235703, 0.235701, 1),
    vec3(0, 0.666666, 2),
    vec3(0.255122, 0.615919, 2),
    vec3(0.471404, 0.471404, 2),
    vec3(0.615919, 0.255123, 2),
    vec3(0.666666, 8.4506e-07, 2),
    vec3(0.615919, -0.255121, 2),
    vec3(0.471405, -0.471403, 2),
    vec3(0.255123, -0.615918, 2),
    vec3(1.69012e-06, -0.666666, 2),
    vec3(-0.25512, -0.61592, 2),
    vec3(-0.471403, -0.471406, 2),
    vec3(-0.615918, -0.255124, 2),
    vec3(-0.666666, -2.53518e-06, 2),
    vec3(-0.61592, 0.255119, 2),
    vec3(-0.471406, 0.471402, 2),
    vec3(-0.255125, 0.615918, 2),
    vec3(0, 1, 3),
    vec3(0.258819, 0.965926, 3),
    vec3(0.5, 0.866026, 3),
    vec3(0.707106, 0.707107, 3),
    vec3(0.866025, 0.500001, 3),
    vec3(0.965926, 0.25882, 3),
    vec3(1, 1.26759e-06, 3),
    vec3(0.965926, -0.258818, 3),
    vec3(0.866026, -0.499999, 3),
    vec3(0.707108, -0.707105, 3),
    vec3(0.500002, -0.866024, 3),
    vec3(0.258821, -0.965925, 3),
    vec3(2.53518e-06, -1, 3),
    vec3(-0.258817, -0.965926, 3),
    vec3(-0.499997, -0.866027, 3),
    vec3(-0.707105, -0.707109, 3),
    vec3(-0.866024, -0.500003, 3),
    vec3(-0.965925, -0.258823, 3),
    vec3(-1, -3.80277e-06, 3),
    vec3(-0.965927, 0.258815, 3),
    vec3(-0.866028, 0.499996, 3),
    vec3(-0.70711, 0.707104, 3),
    vec3(-0.500004, 0.866023, 3),
    vec3(-0.258824, 0.965925, 3));

// 1.5 rotations for a more symmetric coverage
// NOTE: The center tap is not included in the spiral patterns!
const vec2 spiral8Pi3[8] = vec2[](vec2(1, 0), vec2(0.334848, 0.808395), vec2(-0.53033, 0.53033),
                                  vec2(-0.577425, -0.239177), vec2(5.96244e-09, -0.5), vec2(0.346455, -0.143506),
                                  vec2(0.176777, 0.176777), vec2(-0.0478354, 0.115485));

const uint DOF_RANDOM_DIM = 3u;

uint get_pixel_seed(const ivec2 icoord, const uint frame_seed) {
    return hash(hash(uint(icoord.x) + (uint(icoord.y) << 16)) ^ frame_seed);
}

#endif // DOF_COMMON_GLSL
