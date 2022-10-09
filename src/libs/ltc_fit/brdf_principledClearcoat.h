#pragma once

#include "brdf.h"

class BrdfPrincipledClearcoat : public Brdf {
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

    static float D_GTR1(float NDotH, float a) {
        if (a >= 1.0f) {
            return 1.0f / PI;
        }
        const float a2 = a * a;
        const float t = 1.0f + (a2 - 1.0f) * NDotH * NDotH;
        return (a2 - 1.0f) / (PI * std::log(a2) * t);
    }

    static float fresnel_dielectric_cos(float cosi, float eta) {
        // compute fresnel reflectance without explicitly computing the refracted direction
        float c = std::abs(cosi);
        float g = eta * eta - 1 + c * c;
        float result;

        if (g > 0) {
            g = std::sqrt(g);
            float A = (g - c) / (g + c);
            float B = (c * (g + c) - 1) / (c * (g - c) + 1);
            result = 0.5f * A * A * (1 + B * B);
        } else {
            result = 1.0f; // TIR (no refracted component)
        }

        return result;
    }

    static Vec3f sample_GTR1(const float rgh, const float r1, const float r2) {
        const float a = rgh;
        //std::min(std::max(0.001f, rgh), 0.999f);
        const float a2 = a * a;

        const float phi = r1 * (2.0f * PI);

        const float cosTheta = std::sqrt(std::max(0.0f, 1.0f - std::pow(a2, 1.0f - r2)) / (1.0f - a2));
        const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - (cosTheta * cosTheta)));
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        return Vec3f{sinTheta * cosPhi, sinTheta * sinPhi, cosTheta};
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
    virtual float eval(const Vec3f &V, const Vec3f &L, float alpha, float &pdf) const {
        if (V[2] <= 0) {
            pdf = 0;
            return 0;
        }

        alpha = std::min(alpha, 0.9999f);

        const Vec3f H = Normalize(V + L);
        /*if (H[2] <= 0) {
            pdf = 0;
            return 0;
        }*/

        float D = D_GTR1(H[2], alpha);

        if (alpha < 0.0002f) {
            D = std::min(D, 1000000.0f);
        } else {
            D = std::min(D, 10000.0f);
        }


        const float clearcoat_alpha = (0.25f * 0.25f);
        float G = G1(V, clearcoat_alpha, clearcoat_alpha) * G1(L, clearcoat_alpha, clearcoat_alpha);
        if (L[2] <= 0) {
            G = 0;
        }


        // const float FH =
        //     (fresnel_dielectric_cos(dot(V, H), spec_ior) - spec_F0) / (1.0f - spec_F0);
        Vec3f F = Vec3f{1.0f};
        // mix(spec_col, Vec3f(1.0f), FH);

        const float denom = 4.0f * std::abs(V[2] * L[2]);
        F *= (denom != 0.0f) ? (D * G / denom) : 0.0f;

#if 0
        pdf = D * G1(V, alpha, alpha) * std::max(Dot(V, H), 0.0f) / std::abs(V[2]);
        const float div = 4.0f * Dot(V, H);
        if (div != 0.0f) {
            pdf /= div;
        }
#else
        //pdf = D * H[2] / (4.0f * Dot(V, H));
        pdf = D / (4.0f * Dot(V, H));

        //pdf = L[2] / 3.14159f;
#endif

        F *= std::max(L[2], 0.0f);

        //if (std::isnan(F[0])) {
        //    __debugbreak();
        //}

        return F[0];
    }

    virtual Vec3f sample(const Vec3f &V, float alpha, const float U1, const float U2) const {
        if (alpha <= 0.0001f) {
            auto H = Vec3f{0.0f, 0.0f, 1.0f};

            const float dot_N_V = -Dot(H, V);
            return Normalize(reflect(-V, H, dot_N_V));
        }

        alpha = std::min(alpha, 0.9999f);

        /*if (alpha == 1.0f) {
            const float r = sqrtf(U1);
            const float phi = 2.0f * 3.14159f * U2;
            const Vec3f L = Vec3f(r * cosf(phi), r * sinf(phi), sqrtf(1.0f - r * r));
            return L;
        }*/

#if 0
        const float phi = 2.0f * 3.14159f * U1;
        const float r = alpha * sqrtf(U2 / (1.0f - U2));
        const Vec3f N = Normalize(Vec3f(r * cosf(phi), r * sinf(phi), 1.0f));
        const Vec3f L = -V + 2.0f * N * Dot(N, V);
        return L;
#else
        const Vec3f H = sample_GTR1(alpha, U1, U2);
        //const Vec3f H = SampleGGX_VNDF(V, alpha, alpha, U1, U2);

        const float dot_N_V = -Dot(H, V);
        const Vec3f reflected_dir_ts = Normalize(reflect(-V, H, dot_N_V));

        return reflected_dir_ts;
#endif
    }
};
