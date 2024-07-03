
#include "_common.glsl"

#if defined(VULKAN)
#define GetCellIndex(ix, iy, slice, res) \
    (slice * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + ((int(res.y) - 1 - iy) * ITEM_GRID_RES_Y / int(res.y)) * ITEM_GRID_RES_X + ix * ITEM_GRID_RES_X / int(res.x))
#else
#define GetCellIndex(ix, iy, slice, res) \
    (slice * ITEM_GRID_RES_X * ITEM_GRID_RES_Y + (iy * ITEM_GRID_RES_Y / int(res.y)) * ITEM_GRID_RES_X + ix * ITEM_GRID_RES_X / int(res.x))
#endif

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

 const vec2 g_poisson_disk_8[8] = vec2[8](
    vec2(-0.4425296, -0.1756687),
    vec2(-0.1982351, 0.7117441),
    vec2(-0.8524727, 0.4356169),
    vec2(0.3295574, -0.5738078),
    vec2(0.2541587, 0.01586004),
    vec2(-0.263842, -0.8562326),
    vec2(0.8631233, -0.1870599),
    vec2(0.3779924, 0.5789691)
 );

const vec2 g_poisson_disk_16[16] = vec2[16](
    vec2(-0.5370122, 0.3347947),
    vec2(-0.8065823, -0.004128887),
    vec2(-0.161181, 0.7464678),
    vec2(-0.1878462, -0.3402194),
    vec2(0.09400224, 0.2233346),
    vec2(-0.6080497, 0.7578315),
    vec2(0.6229662, 0.6039891),
    vec2(-0.7179142, -0.4770949),
    vec2(0.2477834, -0.4138957),
    vec2(-0.4235222, -0.8372488),
    vec2(0.04635308, -0.7909735),
    vec2(0.6028547, -0.1137719),
    vec2(0.2553098, 0.8434128),
    vec2(0.6131164, -0.6941585),
    vec2(0.9397567, 0.2964764),
    vec2(0.8962983, -0.4016006)
);

const vec2 g_poisson_disk_32[32] = vec2[32](
    vec2(0.06407013, 0.05409927),
	vec2(0.7366577, 0.5789394),
	vec2(-0.6270542, -0.5320278),
	vec2(-0.4096107, 0.8411095),
	vec2(0.6849564, -0.4990818),
	vec2(-0.874181, -0.04579735),
	vec2(0.9989998, 0.0009880066),
	vec2(-0.004920578, -0.9151649),
	vec2(0.1805763, 0.9747483),
	vec2(-0.2138451, 0.2635818),
	vec2(0.109845, 0.3884785),
	vec2(0.06876755, -0.3581074),
	vec2(0.374073, -0.7661266),
	vec2(0.3079132, -0.1216763),
	vec2(-0.3794335, -0.8271583),
	vec2(-0.203878, -0.07715034),
	vec2(0.5912697, 0.1469799),
	vec2(-0.88069, 0.3031784),
	vec2(0.5040108, 0.8283722),
	vec2(-0.5844124, 0.5494877),
	vec2(0.6017799, -0.1726654),
	vec2(-0.5554981, 0.1559997),
	vec2(-0.3016369, -0.3900928),
	vec2(-0.5550632, -0.1723762),
	vec2(0.925029, 0.2995041),
	vec2(-0.2473137, 0.5538505),
	vec2(0.9183037, -0.2862392),
	vec2(0.2469421, 0.6718712),
	vec2(0.3916397, -0.4328209),
	vec2(-0.03576927, -0.6220032),
	vec2(-0.04661255, 0.7995201),
	vec2(0.4402924, 0.3640312)
);

// http://the-witness.net/news/2013/09/shadow-mapping-summary-part-1/
vec2 get_shadow_offsets(const float dot_N_L) {
    const float cos_alpha = saturate(dot_N_L);
    const float offset_scale_N = sqrt(1.0 - cos_alpha * cos_alpha); // sin(acos(L·N))
    const float offset_scale_L = offset_scale_N / cos_alpha;        // tan(acos(L·N))
    return vec2(offset_scale_N, min(2.0, offset_scale_L));
}

// http://the-witness.net/news/2013/09/shadow-mapping-summary-part-1/
float SampleShadowPCF5x5(sampler2DShadow shadow_tex, vec3 shadow_coord) {
    const vec2 shadow_size = vec2(float(SHADOWMAP_RES), float(SHADOWMAP_RES) / 2.0);
    const vec2 shadow_size_inv = vec2(1.0) / shadow_size;

    float z = shadow_coord.z;
    vec2 uv = shadow_coord.xy * shadow_size;
    vec2 base_uv = floor(uv + 0.5);
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

    sum += uw0 * vw0 * texture(shadow_tex, vec3(u0, v0, z));
    sum += uw1 * vw0 * texture(shadow_tex, vec3(u1, v0, z));
    sum += uw2 * vw0 * texture(shadow_tex, vec3(u2, v0, z));

    sum += uw0 * vw1 * texture(shadow_tex, vec3(u0, v1, z));
    sum += uw1 * vw1 * texture(shadow_tex, vec3(u1, v1, z));
    sum += uw2 * vw1 * texture(shadow_tex, vec3(u2, v1, z));

    sum += uw0 * vw2 * texture(shadow_tex, vec3(u0, v2, z));
    sum += uw1 * vw2 * texture(shadow_tex, vec3(u1, v2, z));
    sum += uw2 * vw2 * texture(shadow_tex, vec3(u2, v2, z));

    sum *= (1.0 / 144.0);

    return sum * sum;
}

float GetSunVisibility(float frag_depth, sampler2DShadow shadow_tex, in vec3 aVertexShUVs[4]) {
    float visibility = 0.0;

    if (frag_depth < SHADOWMAP_CASCADE0_DIST) {
        visibility = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[0]);

#if SHADOWMAP_CASCADE_SOFT
        if (frag_depth > 0.9 * SHADOWMAP_CASCADE0_DIST) {
            const float v2 = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[1]);

            const float k = 10.0 * (frag_depth / SHADOWMAP_CASCADE0_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else if (frag_depth < SHADOWMAP_CASCADE1_DIST) {
        visibility = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[1]);

#if SHADOWMAP_CASCADE_SOFT
        if (frag_depth > 0.9 * SHADOWMAP_CASCADE1_DIST) {
            const float v2 = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[2]);

            const float k = 10.0 * (frag_depth / SHADOWMAP_CASCADE1_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else if (frag_depth < SHADOWMAP_CASCADE2_DIST) {
        visibility = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[2]);

#if SHADOWMAP_CASCADE_SOFT
        if (frag_depth > 0.9 * SHADOWMAP_CASCADE2_DIST) {
            const float v2 = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[3]);

            const float k = 10.0 * (frag_depth / SHADOWMAP_CASCADE2_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else {
        visibility = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[3]);
    }

    return visibility;
}

float GetSunVisibility(float frag_depth, sampler2DShadow shadow_tex, in mat4x3 aVertexShUVs) {
    float visibility = 0.0;

    if (frag_depth < SHADOWMAP_CASCADE0_DIST) {
        visibility = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[0]);

#if SHADOWMAP_CASCADE_SOFT
        if (frag_depth > 0.9 * SHADOWMAP_CASCADE0_DIST) {
            const float v2 = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[1]);

            const float k = 10.0 * (frag_depth / SHADOWMAP_CASCADE0_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else if (frag_depth < SHADOWMAP_CASCADE1_DIST) {
        visibility = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[1]);

#if SHADOWMAP_CASCADE_SOFT
        if (frag_depth > 0.9 * SHADOWMAP_CASCADE1_DIST) {
            const float v2 = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[2]);

            const float k = 10.0 * (frag_depth / SHADOWMAP_CASCADE1_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else if (frag_depth < SHADOWMAP_CASCADE2_DIST) {
        visibility = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[2]);

#if SHADOWMAP_CASCADE_SOFT
        if (frag_depth > 0.9 * SHADOWMAP_CASCADE2_DIST) {
            const float v2 = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[3]);

            const float k = 10.0 * (frag_depth / SHADOWMAP_CASCADE2_DIST - 0.9);
            visibility = mix(visibility, v2, k);
        }
#endif
    } else {
        visibility = SampleShadowPCF5x5(shadow_tex, aVertexShUVs[3]);
    }

    return visibility;
}

#define ROTATE_BLOCKER_SEARCH 1
#define GATHER4_BLOCKER_SEARCH 1

vec2 BlockerSearch(sampler2D shadow_val_tex, vec3 shadow_uv, float search_radius, vec4 rotator) {
    const vec2 shadow_size = vec2(float(SHADOWMAP_RES), float(SHADOWMAP_RES) / 2.0);

    float avg_distance = 0.0;
    float count = 0.0;

#if !ROTATE_BLOCKER_SEARCH
    rotator = vec4(1, 0, 0, 1);
#endif

    for (int i = 0; i < 8; ++i) {
        vec2 uv = shadow_uv.xy + search_radius * RotateVector(rotator, g_poisson_disk_8[i] / shadow_size);
#if GATHER4_BLOCKER_SEARCH
        vec4 depth = textureGather(shadow_val_tex, uv, 0);
        vec4 valid = vec4(lessThan(depth, vec4(shadow_uv.z)));
        avg_distance += dot(valid, depth);
        count += dot(valid, vec4(1.0));
#else
        float depth = textureLod(shadow_val_tex, uv, 0.0).x;
        if (depth < shadow_uv.z) {
            avg_distance += depth;
            count += 1.0;
        }
#endif
    }

    return vec2(avg_distance, count);
}

float GetCascadeVisibility(int cascade, sampler2DShadow shadow_tex, sampler2D shadow_val_tex, mat4x3 aVertexShUVs, vec4 rotator, float softness_factor) {
    const vec2 ShadowSizePx = vec2(float(SHADOWMAP_RES), float(SHADOWMAP_RES) / 2.0);
    const float MinShadowRadiusPx = 1.5; // needed to hide blockyness
    const float MaxShadowRadiusPx = 16.0;

    float visibility = 0.0;

    vec2 blocker = BlockerSearch(shadow_val_tex, aVertexShUVs[cascade], MaxShadowRadiusPx, rotator);
    if (blocker.y < 0.5) {
        return 1.0;
    }
    blocker.x /= blocker.y;

    const float filter_radius_px = clamp(softness_factor * abs(blocker.x - aVertexShUVs[cascade].z), MinShadowRadiusPx, MaxShadowRadiusPx);
    for (int i = 0; i < 16; ++i) {
        vec2 uv = aVertexShUVs[cascade].xy + filter_radius_px * RotateVector(rotator, g_poisson_disk_16[i] / ShadowSizePx);
        visibility += textureLod(shadow_tex, vec3(uv, aVertexShUVs[cascade].z), 0.0);
    }
    visibility /= 16.0;

    return visibility;
}

float GetSunVisibility(float frag_depth, sampler2DShadow shadow_tex, sampler2D shadow_val_tex, mat4x3 aVertexShUVs, vec4 softness_factor, float hash) {
    const float angle = hash * 2.0 * M_PI;
    const float ca = cos(angle), sa = sin(angle);
    const vec4 rotator = vec4(ca, sa, -sa, ca);

    if (frag_depth < SHADOWMAP_CASCADE0_DIST) {
        return GetCascadeVisibility(0, shadow_tex, shadow_val_tex, aVertexShUVs, rotator, softness_factor[0]);
    } else if (frag_depth < SHADOWMAP_CASCADE1_DIST) {
        return GetCascadeVisibility(1, shadow_tex, shadow_val_tex, aVertexShUVs, rotator, softness_factor[1]);
    } else if (frag_depth < SHADOWMAP_CASCADE2_DIST) {
        return GetCascadeVisibility(2, shadow_tex, shadow_val_tex, aVertexShUVs, rotator, softness_factor[2]);
    }
    return GetCascadeVisibility(3, shadow_tex, shadow_val_tex, aVertexShUVs, rotator, softness_factor[3]);
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
    float L21D11 = fma(-b[0], b[1], b[2]);
    float D11 = fma(-b[0],b[0], b[1]);
    float InvD11 = 1.0 / D11;
    float L21 = L21D11 * InvD11;
    float SquaredDepthVariance = fma(-b[1],b[1], b[3]);
    float D22 = fma(-L21D11, L21, SquaredDepthVariance);

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
