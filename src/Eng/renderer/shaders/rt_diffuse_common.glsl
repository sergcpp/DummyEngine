#ifndef GI_COMMON_GLSL
#define GI_COMMON_GLSL

bool IsDiffuseSurface(sampler2D depth_tex, usampler2D specular_tex, ivec2 px_coords) {
    const float depth = texelFetch(depth_tex, px_coords, 0).x;
    if (depth > 0.0) {
        const uint packed_mat_params = texelFetch(specular_tex, px_coords, 0).x;
        vec4 mat_params0, mat_params1;
        UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);
        return mat_params1.x < 0.95; // non-metallic
    }
    return false;
}

bool IsDiffuseSurface(float depth_fetch, usampler2D specular_tex, vec2 uv) {
    if (depth_fetch > 0.0) {
        const uint packed_mat_params = textureLod(specular_tex, uv, 0.0).x;
        vec4 mat_params0, mat_params1;
        UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);
        return mat_params1.x < 0.95; // non-metallic
    }
    return false;
}

float GetSpecLobeTanHalfAngle(const float roughness, const float percent_of_volume) {
    return roughness * roughness * percent_of_volume / (1.0 - percent_of_volume + 0.001);
}

float GetNormalWeightParam(const float non_linear_accum_speed, const float lobe_angle_fraction, const float roughness) {
    const float percent_of_volume = 0.75 * mix(saturate(lobe_angle_fraction), 1.0, non_linear_accum_speed);
    const float tan_half_angle = GetSpecLobeTanHalfAngle(roughness, percent_of_volume);
    const float angle = max(atan(tan_half_angle), 0.001);
    return 1.0 / angle;
}

vec2 GetGeometryWeightParams(const float plane_dist_sensitivity, const vec3 point_vs, const vec3 normal_vs, const float non_linear_accum_speed) {
    const float relaxation = mix(1.0, 0.25, non_linear_accum_speed);
    const float a = relaxation / plane_dist_sensitivity;
    const float b = -dot(normal_vs, point_vs) * a;

    return vec2(a, b);
}

// Acos(x) (approximate)
// http://fooplot.com/#W3sidHlwZSI6MCwiZXEiOiJhY29zKHgpIiwiY29sb3IiOiIjMDAwMDAwIn0seyJ0eXBlIjowLCJlcSI6InNxcnQoMS14KSpzcXJ0KDIpIiwiY29sb3IiOiIjRjIwQzBDIn0seyJ0eXBlIjoxMDAwLCJ3aW5kb3ciOlsiMCIsIjEiLCIwIiwiMiJdLCJzaXplIjpbMTE1MCw5MDBdfV0-
#define _AcosApprox(x) (SQRT_2 * sqrt(saturate(1.0 - (x))))

float GetEdgeStoppingNormalWeight(const float px, const float py, const vec3 n1, const vec3 n2) {
    const float angle = _AcosApprox(dot(n1, n2));
    return smoothstep(1.0, 0.0, abs(angle * px + py));
}

/* fp16 */ float GetEdgeStoppingPlanarDistanceWeight(const vec2 geometry_weight_params, const vec3 center_normal_vs, const vec3 neighbor_point_vs) {
    const float d = dot(center_normal_vs, neighbor_point_vs);
    return SmoothStep01(1.0 - abs(d * geometry_weight_params.x + geometry_weight_params.y));
}

uint PackRay(const uvec2 ray_coord, const bool copy_horizontal, const bool copy_vertical, const bool copy_diagonal) {
    const uint ray_x_15bit = ray_coord.x & 32767u; // 0b111111111111111
    const uint ray_y_14bit = ray_coord.y & 16383u; // 0b11111111111111
    const uint copy_horizontal_1bit = copy_horizontal ? 1u : 0u;
    const uint copy_vertical_1bit = copy_vertical ? 1u : 0u;
    const uint copy_diagonal_1bit = copy_diagonal ? 1u : 0u;

    return (copy_diagonal_1bit << 31u) | (copy_vertical_1bit << 30u) | (copy_horizontal_1bit << 29u) | (ray_y_14bit << 15u) | (ray_x_15bit << 0u);
}

void UnpackRayCoords(const uint packed_ray, out uvec2 ray_coord, out bool copy_horizontal, out bool copy_vertical, out bool copy_diagonal) {
    ray_coord.x = (packed_ray >> 0u) & 32767u; // 0b111111111111111
    ray_coord.y = (packed_ray >> 15u) & 16383u; // 0b11111111111111
    copy_horizontal = ((packed_ray >> 29u) & 1u) != 0u; // 0b1
    copy_vertical = ((packed_ray >> 30u) & 1u) != 0u; // 0b1
    copy_diagonal = ((packed_ray >> 31u) & 1u) != 0u; // 0b1
}

vec3 SampleCosineHemisphere(const float u, const float v) {
    const float phi = 2.0 * M_PI * v;

    const float cos_phi = cos(phi);
    const float sin_phi = sin(phi);

    const float dir = sqrt(u);
    const float k = sqrt(1.0 - u);
    return vec3(dir * cos_phi, dir * sin_phi, k);
}

mat3 CreateTBN(vec3 N) {
    vec3 U;
    if (abs(N.z) > 0.0) {
        const float k = sqrt(N.y * N.y + N.z * N.z);
        U.x = 0.0; U.y = -N.z / k; U.z = N.y / k;
    } else {
        const float k = sqrt(N.x * N.x + N.y * N.y);
        U.x = N.y / k; U.y = -N.x / k; U.z = 0.0;
    }

    mat3 TBN;
    TBN[0] = U;
    TBN[1] = cross(N, U);
    TBN[2] = N;
    return transpose(TBN);
}

vec3 SampleDiffuseVector(const sampler2D noise_tex, const vec3 normal, const ivec2 dispatch_thread_id, const int bounce) {
    const vec4 fetch = texelFetch(noise_tex, dispatch_thread_id % 128, 0);

    const vec2 u = (bounce == 0) ? fetch.xy : fetch.zw;
    const vec3 direction_tbn = SampleCosineHemisphere(u.x, u.y);

    const mat3 inv_tbn_transform = transpose(CreateTBN(normal));
    return normalize(inv_tbn_transform * direction_tbn);
}

/* fp16 */ float Luminance(/* fp16 */ const vec3 color) { return max(lum(color), 0.001); }

/* fp16 */ float ComputeTemporalVariance(/* fp16 */ const vec3 history_radiance, /* fp16 */ const vec3 radiance) {
    /* fp16 */ const float history_luminance = Luminance(history_radiance);
    /* fp16 */ const float luminance = Luminance(radiance);
    /* fp16 */ const float diff = abs(history_luminance - luminance) / max(max(history_luminance, luminance), 0.5);
    return diff * diff;
}

vec4 GetBlurKernelRotation(const uvec2 pixel_pos, const vec4 base_rotator, const uint frame) {
    vec4 rotator = vec4(1, 0, 0, 1);

#ifdef PER_PIXEL_KERNEL_ROTATION
    const float angle = Bayer4x4(pixel_pos, frame) * 2.0 * M_PI;

    const float ca = cos(angle);
    const float sa = sin(angle);

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

const uint MAX_DIFFUSE_SAMPLES = 32;

#endif // GI_COMMON_GLSL