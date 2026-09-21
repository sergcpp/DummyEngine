#ifndef _BRDF_OREN_NAYAR_
#define _BRDF_OREN_NAYAR_

#include "brdf.h"

// Single-scatter Oren-Nayar lobe only. The albedo-dependent part of the multi-scatter
// compensation is nonlinear/per-channel and cannot be baked into the scalar LTC factors;
// only the albedo-free energy terms E/Eavg are baked (tex2.y/tex2.z) via compute_E/compute_Eavg.
class BrdfOrenNayar : public Brdf {
  public:
    virtual float eval(const Vec3f &V, const Vec3f &L, const float alpha, float &pdf) const {
        if (V[2] <= 0 || L[2] <= 0) {
            pdf = 0;
            return 0;
        }

        pdf = L[2] / 3.14159f;

        const float sigma = sqrtf(alpha);
        const float a = 1.0f / (3.14159f + sigma * (0.5f * 3.14159f - 2.0f / 3.0f));
        const float b = sigma * a;

        const float nl = L[2];
        const float nv = V[2];

        float t = Dot(L, V) - nl * nv;
        if (t > 0.0f) {
            t /= (nl > nv ? nl : nv) + 1e-38f;
        }

        return nl * (a + b * t);
    }

    virtual Vec3f sample(const Vec3f &V, const float alpha, const float U1, const float U2) const {
        const float r = sqrtf(U1);
        const float phi = 2.0f * 3.14159f * U2;
        const Vec3f L = Vec3f(r * cosf(phi), r * sinf(phi), sqrtf(1.0f - r * r));
        return L;
    }

    virtual bool supports_ltc_energy() const { return true; }

    virtual float compute_E(const float nv, const float sigma) const {
        const float a = 1.0f / (3.14159f + sigma * (0.5f * 3.14159f - 2.0f / 3.0f));
        return a * 3.14159f + sigma * a * oren_nayar_G(nv);
    }

    virtual float compute_Eavg(const float sigma) const {
        const float a = 1.0f / (3.14159f + sigma * (0.5f * 3.14159f - 2.0f / 3.0f));
        return a * 3.14159f + ((2.0f * 3.14159f - 5.6f) / 3.0f) * sigma * a;
    }

  private:
    static float oren_nayar_G(const float cos_theta) {
        if (cos_theta < 1e-6f) {
            return (0.5f * 3.14159f - 2.0f / 3.0f) - cos_theta;
        }
        const float sin_theta = sqrtf(fmaxf(1.0f - cos_theta * cos_theta, 0.0f));
        const float theta = acosf(cos_theta < -1.0f ? -1.0f : (cos_theta > 1.0f ? 1.0f : cos_theta));
        return sin_theta * (theta - 2.0f / 3.0f - sin_theta * cos_theta) +
               2.0f / 3.0f * (sin_theta / cos_theta) * (1.0f - sin_theta * sin_theta * sin_theta);
    }
};

#endif
