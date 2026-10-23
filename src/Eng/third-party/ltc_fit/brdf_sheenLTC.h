#ifndef _BRDF_SHEEN_LTC_
#define _BRDF_SHEEN_LTC_

#include "brdf.h"

// Analytic approximation of the volumetric sheen layer from
// "Practical Multiple-Scattering Sheen Using Linearly Transformed Cosines"
// (Zeltner, Burley, Chiang, SIGGRAPH 2022): a fiber-oriented SGGX microflake
// layer, with single scattering evaluated exactly and multiple scattering
// approximated by an empirical fit to a reference random walk.
// Ported from https://github.com/tizian/ltc-sheen (fitting/src/bsdfs/sheen_approx.h).
class BrdfSheenLTC : public Brdf {
    static constexpr float PI = 3.14159265358979f;

    // The general SGGX ellipsoid collapses to diag(1, 1, sigma^2) for a fiber
    // aligned with the shading normal (the Z axis), which is the only case
    // needed here.
    static float sggx_sigma(const Vec3f &w, const float sigma2) {
        const float s = w[0] * w[0] + w[1] * w[1] + sigma2 * w[2] * w[2];
        return std::sqrt(std::max(0.0f, s));
    }

    static float sggx_ndf(const Vec3f &wm, const float sigma, const float sigma2) {
        const float den = sigma2 + wm[2] * wm[2] * (1.0f - sigma2);
        // detS = sigma^2, so detS^1.5 == sigma * sigma2
        return (sigma * sigma2) / (PI * den * den);
    }

    static float sggx_phase_specular(const Vec3f &wi, const Vec3f &wo, const float sigma, const float sigma2) {
        const Vec3f wh = Normalize(wi + wo);
        const float s = sggx_sigma(wi, sigma2);
        return s == 0.0f ? 0.0f : 0.25f * sggx_ndf(wh, sigma, sigma2) / s;
    }

    static float Ft(const float sigma, const float c) {
        const float b = std::max(0.55f, 0.44f + 0.42f * sigma);
        return b + (1.0f - b) * (1.0f - std::pow(1.0f - c, 12.0f));
    }

    // Bicubic B-spline interpolation of the fitted `m`/`b` values, sampled at
    // sigma = 0, 0.1, ..., 1.0 (first/last entries extrapolated so the spline
    // interpolates the end points).
    static void mb(const float sigma, float &m, float &b) {
        constexpr float mv0 = 2.0f * 20.0f - 10.2f, mv12 = 2.0f * 0.95f - 1.1f;
        constexpr float mvals[13] = {mv0,  20.0f, 10.2f, 6.14f, 4.30f, 3.30f, 2.55f,
                                      2.02f, 1.63f, 1.33f, 1.1f,  0.95f, mv12};
        constexpr float bv0 = 2.0f * 3.7f - 2.23f, bv12 = 2.0f * -1.65f + 1.55f;
        constexpr float bvals[13] = {bv0,    3.7f,   2.23f,  1.06f,  0.293f, -0.212f, -0.641f,
                                      -0.952f, -1.2f, -1.4f, -1.55f, -1.65f, bv12};

        const float r = sigma * 10.0f;
        const float ri = std::min(std::floor(r), 9.0f);
        const int i = int(ri);
        const float m0 = mvals[i], m1 = mvals[i + 1], m2 = mvals[i + 2], m3 = mvals[i + 3];
        const float b0 = bvals[i], b1 = bvals[i + 1], b2 = bvals[i + 2], b3 = bvals[i + 3];

        const float y = r - ri, y2 = y * y, y3 = y2 * y;
        const float B0 = -y + 2.0f * y2 - y3;
        const float B1 = 2.0f - 5.0f * y2 + 3.0f * y3;
        const float B2 = y + 4.0f * y2 - 3.0f * y3;
        const float B3 = -y2 + y3;

        m = 0.5f * (B0 * m0 + B1 * m1 + B2 * m2 + B3 * m3);
        b = 0.5f * (B0 * b0 + B1 * b1 + B2 * b2 + B3 * b3);
    }

  public:
    // NOTE: this tool's `alpha` convention is roughness^2 (see fitTab() in
    // fitLTC.cpp), which is exactly the SGGX fiber cross section `sigma` used
    // by the reference (their own `alpha` is roughness, and sigma = alpha^2).
    virtual float eval(const Vec3f &V, const Vec3f &L, const float alpha, float &pdf) const {
        if (V[2] <= 0.0f || L[2] < 0.0f) {
            pdf = 0;
            return 0;
        }

        pdf = L[2] / PI;

        const float sigma = alpha;
        const float sigma2 = sigma * sigma;

        const float cos_theta_i = V[2], cos_theta_o = L[2];

        // single scattering (exact)
        const float lambda_wi = sggx_sigma(V, sigma2) / cos_theta_i;
        const float lambda_wo = sggx_sigma(L, sigma2) / cos_theta_o;

        const float tmp0 = lambda_wi + lambda_wo;
        const float tmp1 = 1.0f - std::exp(-tmp0);
        const float tmp = tmp1 / (std::abs(cos_theta_i) * tmp0);

        const float sigma_t_along_wi = sggx_sigma(V, sigma2);
        const float phase_function = sggx_phase_specular(V, L, sigma, sigma2);

        float value = phase_function * sigma_t_along_wi * tmp;

        if (!std::isfinite(value)) {
            // lambda_wi/lambda_wo blow up at grazing angles
            value = 0.0f;
        }

        // multiple scattering (empirical fit to a reference random walk)
        float m, b;
        mb(sigma, m, b);
        float ms = std::exp(b - m * std::sqrt(cos_theta_o * cos_theta_o + cos_theta_i * cos_theta_i));
        ms *= Ft(sigma, cos_theta_o) * Ft(sigma, cos_theta_i);
        value += ms * cos_theta_o;

        return value;
    }

    virtual Vec3f sample(const Vec3f &V, const float alpha, const float U1, const float U2) const {
        // The reference BRDF has no closed-form importance sampling; fall
        // back to plain cosine-weighted hemisphere sampling (matches the pdf
        // used in eval() above).
        const float r = std::sqrt(U1);
        const float phi = 2.0f * PI * U2;
        const Vec3f L = Vec3f(r * std::cos(phi), r * std::sin(phi), std::sqrt(1.0f - r * r));
        return L;
    }
};

#endif
