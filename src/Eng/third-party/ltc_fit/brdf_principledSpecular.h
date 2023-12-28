#pragma once

#include "brdf.h"

class BrdfPrincipledSpecular : public Brdf {
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
        const Vec3f T1 =
            lensq > 0.0f ? Vec3f(-Vh[1], Vh[0], 0.0f) / std::sqrt(lensq) : Vec3f(1.0f, 0.0f, 0.0f);
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

    static Vec3f reflect(const Vec3f &I, const Vec3f &N, const float dot_N_I) {
        return I - 2 * dot_N_I * N;
    }

  public:
    virtual float eval(const Vec3f &V, const Vec3f &L, const float alpha, float &pdf) const {
        if (V[2] <= 0) {
            pdf = 0;
            return 0;
        }

#if 0
		// masking
        const float a_V = 1.0f / alpha / tanf(acosf(V[2]));
        const float LambdaV = (V[2] < 1.0f) ? 0.5f * (-1.0f + sqrtf(1.0f + 1.0f / a_V / a_V)) : 0.0f;
		const float G1 = 1.0f / (1.0f + LambdaV);

		// shadowing
		float G2;
        if (L[2] <= 0.0f)
			G2 = 0;
		else
		{
            const float a_L = 1.0f / alpha / tanf(acosf(L[2]));
            const float LambdaL = (L[2] < 1.0f) ? 0.5f * (-1.0f + sqrtf(1.0f + 1.0f / a_L / a_L)) : 0.0f;
			G2 = 1.0f / (1.0f + LambdaV + LambdaL);
		}

		// D
		const Vec3f H = Normalize(V+L);
        const float slopex = H[0] / H[2];
        const float slopey = H[1] / H[2];
		float D = 1.0f / (1.0f + (slopex*slopex+slopey*slopey)/alpha/alpha);
		D = D*D;
        D = D / (3.14159f * alpha * alpha * H[2] * H[2] * H[2] * H[2]);

		pdf = fabsf(D * H[2] / 4.0f / Dot(V, H));
        float res = D * G2 / 4.0f / V[2];

		return res;
#else
        const Vec3f H = Normalize(V + L);
        const float D = D_GGX(H, alpha, alpha);

        const float G = G1(V, alpha, alpha) * G1(L, alpha, alpha);

        // const float FH =
        //     (fresnel_dielectric_cos(dot(V, H), spec_ior) - spec_F0) / (1.0f - spec_F0);
        Vec3f F = Vec3f{1.0f};
        // mix(spec_col, Vec3f(1.0f), FH);

        const float denom = 4.0f * std::abs(V[2] * L[2]);
        F *= (denom != 0.0f) ? (D * G / denom) : 0.0f;

        pdf = D * G1(V, alpha, alpha) * std::max(Dot(V, H), 0.0f) / std::abs(V[2]);
        const float div = 4.0f * Dot(V, H);
        if (div != 0.0f) {
            pdf /= div;
        }

        F *= std::max(L[2], 0.0f);

        return F[0];
#endif
    }

    virtual Vec3f sample(const Vec3f &V, const float alpha, const float U1, const float U2) const {
#if 0
        const float phi = 2.0f * 3.14159f * U1;
        const float r = alpha * sqrtf(U2 / (1.0f - U2));
        const Vec3f N = Normalize(Vec3f(r * cosf(phi), r * sinf(phi), 1.0f));
        const Vec3f L = -V + 2.0f * N * Dot(N, V);
        return L;
#else
        const Vec3f H = SampleGGX_VNDF(V, alpha, alpha, U1, U2);

        const float dot_N_V = -Dot(H, V);
        const Vec3f reflected_dir_ts = Normalize(reflect(-V, H, dot_N_V));

        return reflected_dir_ts;
#endif
    }
};
