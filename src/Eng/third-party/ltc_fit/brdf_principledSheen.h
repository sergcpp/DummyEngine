#ifndef _BRDF_PRINCIPLED_SHEEN_
#define _BRDF_PRINCIPLED_SHEEN_

#include "brdf.h"

class BrdfPrincipledSheen : public Brdf
{
    static float schlick_weight(const float u) {
        const float m = std::min(std::max(1.0f - u, 0.0f), 1.0f);
        return (m * m) * (m * m) * m;
    }

	static float mix(const float v1, const float v2, const float k) { return (1.0f - k) * v1 + k * v2; }

public:

	virtual float eval(const Vec3f& V, const Vec3f& L, const float alpha, float& pdf) const
	{
#if 0
        if (/*V[2] <= 0 ||*/ L[2] <= 0) {
            pdf = 0;
            return 0;
        }

		pdf = L[2] / 3.14159f;

        const float FL = schlick_weight(L[2]);
        const float FV = schlick_weight(V[2]);

		const float roughness = std::sqrt(alpha);

        const float L_dot_H = Dot(L, Normalize(V + L));
        const float Fd90 = 0.5f + 2.0f * L_dot_H * L_dot_H * roughness;
        const float Fd = mix(1.0f, Fd90, FL) * mix(1.0f, Fd90, FV);

		return Fd * L[2] / 3.14159f;
#else
        if (/*V[2] <= 0 ||*/ L[2] <= 0) {
            pdf = 0;
            return 0;
        }

        pdf = L[2] / 3.14159f;

        Vec3f H = Normalize(V + L);
        if (Dot(V, H) < 0.0f) {
            H = -H;
        }

        const float FH = 3.14159f * schlick_weight(Dot(L, H));
        
        return FH * L[2] / 3.14159f;
#endif
	}

	virtual Vec3f sample(const Vec3f& V, const float alpha, const float U1, const float U2) const
	{
		const float r = sqrtf(U1);
		const float phi = 2.0f*3.14159f * U2;
		const Vec3f L = Vec3f(r*cosf(phi), r*sinf(phi), sqrtf(1.0f - r*r));
		return L;
	}	
};


#endif
