#pragma once

#include "brdf.h"

// Single-scatter GGX lobe only. The albedo-dependent part of the multi-scatter compensation is
// nonlinear/per-channel and cannot be baked into the scalar LTC factors; only the albedo-free
// energy terms E/Eavg are baked (tex2.z/tex2.w, see packTab) via compute_E/compute_Eavg, sourced
// from Ray's precomputed GGX directional-albedo table (no closed form exists for GGX, unlike Oren-Nayar).

namespace GGXEnergyPreservingDetail {
#include "__ggx_energy_compensation.inl"

// Bilinear lookup into ggx_E[ndotv_row][rough_col]
float ggx_E_lookup(const float ndotv, const float rough) {
    const int res = 32;
    const float x = std::min(std::max(rough, 0.0f), 1.0f) * float(res - 1);
    const float y = std::min(std::max(ndotv, 0.0f), 1.0f) * float(res - 1);

    const int x0 = std::min(int(x), res - 1), x1 = std::min(x0 + 1, res - 1);
    const int y0 = std::min(int(y), res - 1), y1 = std::min(y0 + 1, res - 1);
    const float tx = x - float(x0), ty = y - float(y0);

    const float c00 = ggx_E[y0][x0], c10 = ggx_E[y0][x1];
    const float c01 = ggx_E[y1][x0], c11 = ggx_E[y1][x1];

    return (1.0f - tx) * (1.0f - ty) * c00 + tx * (1.0f - ty) * c10 + (1.0f - tx) * ty * c01 + tx * ty * c11;
}

// Linear lookup into ggx_Eavg[rough]
float ggx_Eavg_lookup(const float rough) {
    const int res = 32;
    const float x = std::min(std::max(rough, 0.0f), 1.0f) * float(res - 1);

    const int x0 = int(x), x1 = std::min(x0 + 1, res - 1);
    const float tx = x - float(x0);

    return (1.0f - tx) * ggx_Eavg[x0] + tx * ggx_Eavg[x1];
}
} // namespace GGXEnergyPreservingDetail

class BrdfGGX_EnergyPreserving : public Brdf {
    static constexpr float PI = 3.141592653589793238463f;

    static float G1(const Vec3f &Ve, float alpha_x, float alpha_y) {
        alpha_x *= alpha_x;
        alpha_y *= alpha_y;
        const float delta =
            (-1.0f + std::sqrt(1.0f + (alpha_x * Ve[0] * Ve[0] + alpha_y * Ve[1] * Ve[1]) / (Ve[2] * Ve[2]))) / 2.0f;
        return 1.0f / (1.0f + delta);
    }

    static float D_GGX(const Vec3f &H, const float alpha_x, const float alpha_y) {
        if (H[2] == 0.0f) {
            return 0.0f;
        }
        const float sx = -H[0] / (H[2] * alpha_x);
        const float sy = -H[1] / (H[2] * alpha_y);
        const float s1 = 1.0f + sx * sx + sy * sy;
        const float cos_theta_h4 = H[2] * H[2] * H[2] * H[2];
        return 1.0f / ((s1 * s1) * PI * alpha_x * alpha_y * cos_theta_h4);
    }

    // http://jcgt.org/published/0007/04/01/paper.pdf by Eric Heitz
    // Input Ve: view direction
    // Input alpha_x, alpha_y: roughness parameters
    // Input U1, U2: uniform random numbers
    // Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
    static Vec3f SampleGGX_VNDF(const Vec3f &Ve, float alpha_x, float alpha_y, float U1, float U2) {
        // Section 3.2: transforming the view direction to the hemisphere configuration
        const Vec3f Vh = Normalize(Vec3f(alpha_x * Ve[0], alpha_y * Ve[1], Ve[2]));
        // Section 4.1: orthonormal basis (with special case if cross product is zero)
        const float lensq = Vh[0] * Vh[0] + Vh[1] * Vh[1];
        const Vec3f T1 = lensq > 0.0f ? Vec3f(-Vh[1], Vh[0], 0.0f) / std::sqrt(lensq) : Vec3f(1.0f, 0.0f, 0.0f);
        const Vec3f T2 = Cross(Vh, T1);
        // Section 4.2: parameterization of the projected area
        const float r = std::sqrt(U1);
        const float phi = 2.0f * PI * U2;
        const float t1 = r * std::cos(phi);
        float t2 = r * std::sin(phi);
        const float s = 0.5f * (1.0f + Vh[2]);
        t2 = (1.0f - s) * std::sqrt(1.0f - t1 * t1) + s * t2;
        // Section 4.3: reprojection onto hemisphere
        const Vec3f Nh = t1 * T1 + t2 * T2 + std::sqrt(std::max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;
        // Section 3.4: transforming the normal back to the ellipsoid configuration
        const Vec3f Ne = Normalize(Vec3f(alpha_x * Nh[0], alpha_y * Nh[1], std::max(0.0f, Nh[2])));
        return Ne;
    }

    static Vec3f reflect(const Vec3f &I, const Vec3f &N, const float dot_N_I) { return I - 2 * dot_N_I * N; }

  public:
    virtual float eval(const Vec3f &V, const Vec3f &L, const float alpha, float &pdf) const {
        if (V[2] <= 0) {
            pdf = 0;
            return 0;
        }

        const Vec3f H = Normalize(V + L);
        const float D = D_GGX(H, alpha, alpha);

        const float G = G1(V, alpha, alpha) * G1(L, alpha, alpha);

        float F = 1.0f;

        const float denom = 4.0f * std::abs(V[2] * L[2]);
        F *= (denom != 0.0f) ? (D * G / denom) : 0.0f;

        pdf = D * G1(V, alpha, alpha) * std::max(Dot(V, H), 0.0f) / std::abs(V[2]);
        const float div = 4.0f * Dot(V, H);
        if (div != 0.0f) {
            pdf /= div;
        }

        F *= std::max(L[2], 0.0f);

        return F;
    }

    virtual Vec3f sample(const Vec3f &V, const float alpha, const float U1, const float U2) const {
        const Vec3f H = SampleGGX_VNDF(V, alpha, alpha, U1, U2);

        const float dot_N_V = -Dot(H, V);
        const Vec3f reflected_dir_ts = Normalize(reflect(-V, H, dot_N_V));

        return reflected_dir_ts;
    }

    virtual bool supports_ltc_energy() const { return true; }

    // alpha here is already isotropic (alpha_x == alpha_y == alpha), so
    // sqrtf(sqrtf(alpha_x * alpha_y)) collapses to sqrtf(alpha) == sigma
    virtual float compute_E(const float nv, const float sigma) const {
        return GGXEnergyPreservingDetail::ggx_E_lookup(std::max(nv, 0.0f), sigma);
    }

    virtual float compute_Eavg(const float sigma) const { return GGXEnergyPreservingDetail::ggx_Eavg_lookup(sigma); }
};
