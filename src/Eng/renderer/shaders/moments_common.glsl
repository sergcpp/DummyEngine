#ifndef MOMENTS_COMMON_GLSL
#define MOMENTS_COMMON_GLSL

#ifndef MOMENTS_MIN_DIST
	#define MOMENTS_MIN_DIST 1.0
#endif
#ifndef MOMENTS_MAX_DIST
	#define MOMENTS_MAX_DIST 10000.0
#endif

// "Moment-Based Order-Independent Transparency"
// https://momentsingraphics.de/I3D2018.html

float ComputeLogDepth(const float view_z) {
    return (log2(view_z) - log2(MOMENTS_MIN_DIST)) / (log2(MOMENTS_MAX_DIST) - log2(MOMENTS_MIN_DIST));
}

float ComputeAbsorbanceAtDepthFrom4PowerMoments(const float b0, vec4 b1234, const float depth, const float bias, const float overestimation) {
    const vec4 BiasVector = vec4(0.0, 0.628, 0.0, 0.628);

	// Bias input data to avoid artifacts
	b1234 = mix(b1234, BiasVector, bias);
	vec3 z;
	z[0] = depth;

	// Compute a Cholesky factorization of the Hankel matrix B storing only non-
	// trivial entries or related products
	float L21D11 = fma(-b1234[0], b1234[1], b1234[2]);
	float D11 = fma(-b1234[0], b1234[0], b1234[1]);
	float InvD11 = 1.0 / D11;
	float L21 = L21D11 * InvD11;
	float SquaredDepthVariance = fma(-b1234[1], b1234[1], b1234[3]);
	float D22 = fma(-L21D11, L21, SquaredDepthVariance);

	// Obtain a scaled inverse image of bz=(1,z[0],z[0]*z[0])^T
	vec3 c = vec3(1.0, z[0], z[0] * z[0]);
	// Forward substitution to solve L*c1=bz
	c[1] -= b1234.x;
	c[2] -= b1234.y + L21 * c[1];
	// Scaling to solve D*c2=c1
	c[1] *= InvD11;
	c[2] /= D22;
	// Backward substitution to solve L^T*c3=c2
	c[1] -= L21 * c[2];
	c[0] -= dot(c.yz, b1234.xy);
	// Solve the quadratic equation c[0]+c[1]*z+c[2]*z^2 to obtain solutions
	// z[1] and z[2]
	float InvC2 = 1.0 / c[2];
	float p = c[1] * InvC2;
	float q = c[0] * InvC2;
	float D = (p * p * 0.25) - q;
	float r = sqrt(max(D, 0.0001));
	z[1] = -p * 0.5 - r;
	z[2] = -p * 0.5 + r;
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
	polynomial[0] = f0 - polynomial[0] * z[0];
	float absorbance = polynomial[0] + dot(b1234.xy, polynomial.yz);
	// Turn the normalized absorbance into absolute one
	return b0 * absorbance;
}

struct pow_moments4_t {
    float b0;
    vec4 b1234;
};

void UpdateMoments(const float local_absorbance, const float view_z, inout pow_moments4_t inout_moments) {
	const float log_z = ComputeLogDepth(view_z);
    const float depth = 2 * saturate(log_z) - 1;
    const float depth2 = (depth * depth);
    const float depth4 = (depth2 * depth2);

    inout_moments.b0 += local_absorbance;
	inout_moments.b1234 += vec4(depth, depth2, depth2 * depth, depth4) * local_absorbance;
}

// Returns vec2 with x = absorbance at depth, y = total absorbance
vec2 ResolveAbsorbance(const float view_z, const pow_moments4_t moments, const float bias /*= 0.0035*/, const float overestimation /*= 0.25*/) {
    vec2 ret = vec2(0.0);
    if (moments.b0 < 0.001) {
        return ret;
    }

    // NOTE: We could pre-normalize them in shader for better precision, but we need to keep them in proper range for accumulation/upsampling
    const vec4 b1234 = moments.b1234 * rcp(moments.b0);

	const float log_z = ComputeLogDepth(view_z);

    // Total absorbance
    ret.y = moments.b0;
    // Absorbance at requested depth
    ret.x = ComputeAbsorbanceAtDepthFrom4PowerMoments(moments.b0, b1234, log_z, bias,  overestimation);
    ret.x = min(ret.x, ret.y); // Ensure absorbance at depth is never bigger than total absorbance
    return ret;
}

#endif // MOMENTS_COMMON_GLSL