#ifndef _BRDF_BECKMANN_
#define _BRDF_BECKMANN_

#include "brdf.h"

class BrdfBeckmann : public Brdf
{
public:

	virtual float eval(const Vec3f& V, const Vec3f& L, const float alpha, float& pdf) const
	{
      if (V[2] <= 0)
		{
			pdf = 0;
			return 0;
		}

		// masking
        const float a_V = 1.0f / alpha / tanf(acosf(V[2]));
        const float LambdaV =
                    (V[2] < 1.0f) ? (1.0f - 1.259f * a_V + 0.396f * a_V * a_V) / (3.535f * a_V + 2.181f * a_V * a_V)
                                 : 0.0f;
		const float G1 = 1.0f / (1.0f + LambdaV);

		// shadowing
		float G2;
		if(L[2] <= 0.0f)
			G2 = 0;
		else
		{
            const float a_L = 1.0f / alpha / tanf(acosf(L[2]));
            const float LambdaL =
                        (L[2] < 1.0f) ? (1.0f - 1.259f * a_L + 0.396f * a_L * a_L) / (3.535f * a_L + 2.181f * a_L * a_L)
                                     : 0.0f;
			G2 = 1.0f / (1.0f + LambdaV + LambdaL);
		}

		// D
		const Vec3f H = Normalize(V+L);
        const float slopex = H[0] / H[2];
        const float slopey = H[1] / H[2];
        float D = expf(-(slopex * slopex + slopey * slopey) / (alpha * alpha)) /
                  (3.14159f * alpha * alpha * H[2] * H[2] * H[2] * H[2]);

		pdf = fabsf(D * H[2] / 4.0f / Dot(V, H));
        float res = D * G2 / 4.0f / V[2];

		return res;
	}

	virtual Vec3f sample(const Vec3f& V, const float alpha, const float U1, const float U2) const
	{
		const float phi = 2.0f*3.14159f * U1;
		const float r = alpha*sqrtf(-logf(U2));
		const Vec3f N = Normalize(Vec3f(r*cosf(phi), r*sinf(phi), 1.0f));
		const Vec3f L = -V + 2.0f * N * Dot(N, V);
		return L;
	}

};


#endif
