#ifndef SSR_COMMON_GLSL
#define SSR_COMMON_GLSL

const float RoughnessSigmaMin = 0.001;
const float RoughnessSigmaMax = 0.01;

const float SpecularLobeTrim = 0.95;

bool IsReflectiveSurface(sampler2D depth_tex, usampler2D specular_tex, ivec2 px_coords) {
    const float depth = texelFetch(depth_tex, px_coords, 0).r;
    if (depth > 0.0) {
        const uint packed_mat_params = texelFetch(specular_tex, px_coords, 0).r;
        vec4 mat_params0, mat_params1;
        UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);
        return mat_params0.z > 0.0; // has specular
    }
    return false;
}

bool IsReflectiveSurface(float depth_fetch, usampler2D specular_tex, vec2 uv) {
    if (depth_fetch > 0.0) {
        const uint packed_mat_params = textureLod(specular_tex, uv, 0.0).r;
        vec4 mat_params0, mat_params1;
        UnpackMaterialParams(packed_mat_params, mat_params0, mat_params1);
        return mat_params0.z > 0.0; // has specular
    }
    return false;
}

float GetEdgeStoppingNormalWeight(vec3 normal_p, vec3 normal_q, float sigma) {
    return pow(clamp(dot(normal_p, normal_q), 0.0, 1.0), sigma);
}

float GetEdgeStoppingRoughnessWeight(float roughness_p, float roughness_q, float sigma_min, float sigma_max) {
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

uint PackRay(const uvec2 ray_coord, const bool copy_horizontal, const bool copy_vertical, const bool copy_diagonal) {
    const uint ray_x_15bit = ray_coord.x & 32767u; // 0b111111111111111
    const uint ray_y_14bit = ray_coord.y & 16383u; // 0b11111111111111
    const uint copy_horizontal_1bit = copy_horizontal ? 1u : 0u;
    const uint copy_vertical_1bit = copy_vertical ? 1u : 0u;
    const uint copy_diagonal_1bit = copy_diagonal ? 1u : 0u;

    return (copy_diagonal_1bit << 31u) | (copy_vertical_1bit << 30u) | (copy_horizontal_1bit << 29u) | (ray_y_14bit << 15u) | (ray_x_15bit << 0u);
}

uint PackRay(const uvec2 ray_coord, const uint layer_index) {
    const uint ray_x_15bit = ray_coord.x & 32767u; // 0b111111111111111
    const uint ray_y_14bit = ray_coord.y & 16383u; // 0b11111111111111
    const uint layer_3bit = layer_index & 7u; // 0b111

    return (layer_3bit << 29u) | (ray_y_14bit << 15u) | (ray_x_15bit << 0u);
}

void UnpackRayCoords(uint packed_ray, out uvec2 ray_coord, out bool copy_horizontal, out bool copy_vertical, out bool copy_diagonal) {
    ray_coord.x = (packed_ray >> 0u) & 32767u; // 0b111111111111111
    ray_coord.y = (packed_ray >> 15u) & 16383u; // 0b11111111111111
    copy_horizontal = ((packed_ray >> 29u) & 1u) != 0u; // 0b1
    copy_vertical = ((packed_ray >> 30u) & 1u) != 0u; // 0b1
    copy_diagonal = ((packed_ray >> 31u) & 1u) != 0u; // 0b1
}

void UnpackRayCoords(uint packed_ray, out uvec2 ray_coord, out uint layer_index) {
    ray_coord.x = (packed_ray >> 0u) & 32767u; // 0b111111111111111
    ray_coord.y = (packed_ray >> 15u) & 16383u; // 0b11111111111111
    layer_index = (packed_ray >> 29u) & 7u; // 0b111
}

// http://jcgt.org/published/0007/04/01/paper.pdf
vec3 SampleGGXVNDF_CrossSect(const vec3 Vh, const float U1, const float U2) {
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    const float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    const vec3 T1 = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
    const vec3 T2 = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    const float r = sqrt(U1);
    const float phi = 2.0 * M_PI * U2;
    const float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    const float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    const vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    return Nh;
}

// https://arxiv.org/pdf/2306.05044.pdf
vec3 SampleVNDF_Hemisphere_SphCap(const vec3 Vh, const float U1, const float U2) {
    // sample a spherical cap in (-Vh.z, 1]
    const float phi = 2.0f * M_PI * U1;
    const float z = fma(1.0 - U2, 1.0 + Vh.z, -Vh.z);
    const float sin_theta = sqrt(saturate(1.0 - z * z));
    const float x = sin_theta * cos(phi);
    const float y = sin_theta * sin(phi);
    const vec3 c = vec3(x, y, z);
    // normalization will be done later
    return c + Vh;
}

// https://gpuopen.com/download/publications/Bounded_VNDF_Sampling_for_Smith-GGX_Reflections.pdf
vec3 SampleVNDF_Hemisphere_SphCap_Bounded(const vec3 Ve, const vec3 Vh, const vec2 alpha, const float U1, const float U2) {
    // sample a spherical cap in (-Vh.z, 1]
    const float phi = 2.0 * M_PI * U1;
    const float a = saturate(min(alpha.x, alpha.y));
    const float s = 1.0 + length(Ve.xy);
    const float a2 = a * a, s2 = s * s;
    const float k = (1.0 - a2) * s2 / (s2 + a2 * Ve.z * Ve.z);
    const float b = (Ve.z > 0.0) ? k * Vh.z : Vh.z;
    const float z = fma(1.0 - U2, 1.0f + b, -b);
    const float sin_theta = sqrt(saturate(1.0 - z * z));
    const float x = sin_theta * cos(phi);
    const float y = sin_theta * sin(phi);
    const vec3 c = vec3(x, y, z);
    // normalization will be done later
    return c + Vh;
}

vec3 Sample_GGX_VNDF_Ellipsoid(const vec3 Ve, const float alpha_x, const float alpha_y, const float U1, const float U2) {
    const vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    const vec3 Nh = SampleVNDF_Hemisphere_SphCap_Bounded(Ve, Vh, vec2(alpha_x, alpha_y), U1, U2);
    const vec3 Ne = normalize(vec3(alpha_x * Nh[0], alpha_y * Nh[1], max(0.0, Nh[2])));
    return Ne;
}

vec3 Sample_GGX_VNDF_Hemisphere(const vec3 Ve, float alpha, const float U1, const float U2) {
    alpha = max(alpha, 0.00001);
    return Sample_GGX_VNDF_Ellipsoid(Ve, alpha, alpha, U1, U2);
}

float D_GTR2(const float N_dot_H, const float a) {
    const float a2 = (a * a);
    const float t = 1.0 + (a2 - 1.0) * N_dot_H * N_dot_H;
    return a2 / (M_PI * t * t);
}

float D_GGX(const vec3 H, const vec2 alpha) {
    if (H[2] == 0.0) {
        return 0.0;
    }
    const float sx = -H[0] / (H[2] * alpha.x);
    const float sy = -H[1] / (H[2] * alpha.y);
    const float s1 = 1.0 + sx * sx + sy * sy;
    const float cos_theta_h4 = H[2] * H[2] * H[2] * H[2];
    return 1.0 / ((s1 * s1) * M_PI * alpha.x * alpha.y * cos_theta_h4);
}

float GGX_VNDF_Reflection_Bounded_PDF(const float D, const vec3 view_dir_ts, const vec2 alpha) {
    const vec2 ai = alpha * view_dir_ts.xy;
    const float len2 = dot(ai, ai);
    const float t = sqrt(len2 + view_dir_ts.z * view_dir_ts.z);
    if (view_dir_ts.z >= 0.0) {
        const float a = saturate(min(alpha.x, alpha.y));
        const float s = 1.0 + length(view_dir_ts.xy);
        const float a2 = a * a, s2 = s * s;
        const float k = (1.0 - a2) * s2 / (s2 + a2 * view_dir_ts.z * view_dir_ts.z);
        return D / (2.0 * (k * view_dir_ts.z + t));
    }
    return D * (t - view_dir_ts.z) / (2.0 * len2);
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

vec3 SampleReflectionVector(const vec3 view_direction, const vec3 normal, const float roughness, const vec2 u) {
    const mat3 tbn_transform = CreateTBN(normal);
    const vec3 view_direction_tbn = tbn_transform * (-view_direction);

    vec3 sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y * SpecularLobeTrim);
#ifdef PERFECT_REFLECTIONS
    sampled_normal_tbn = vec3(0.0, 0.0, 1.0); // Overwrite normal sample to produce perfect reflection.
#endif

    const vec3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

    // Transform reflected_direction back to the initial space.
    const mat3 inv_tbn_transform = transpose(tbn_transform);
    return (inv_tbn_transform * reflected_direction_tbn);
}

/* fp16 */ float Luminance(/* fp16 */ vec3 color) { return max(dot(color, vec3(0.299, 0.587, 0.114)), 0.001); }

/* fp16 */ float ComputeTemporalVariance(/* fp16 */ vec3 history_radiance, /* fp16 */ vec3 radiance) {
    /* fp16 */ float history_luminance = Luminance(history_radiance);
    /* fp16 */ float luminance = Luminance(radiance);
    /* fp16 */ float diff = abs(history_luminance - luminance) / max(max(history_luminance, luminance), 0.5);
    return diff * diff;
}

bool IsMirrorReflection(float roughness) {
    return roughness <= 0.0001;
}

bool IsGlossyReflection(float roughness) {
    return true;
}

#endif // SSR_COMMON_GLSL