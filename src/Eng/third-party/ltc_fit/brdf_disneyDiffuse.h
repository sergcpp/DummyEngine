#ifndef _BRDF_DISNEY_DIFFUSE_
#define _BRDF_DISNEY_DIFFUSE_

#include "brdf.h"

class BrdfDisneyDiffuse : public Brdf
{
public:

	virtual float eval(const Vec3f& V, const Vec3f& L, const float alpha, float& pdf) const
	{
      if (V[2] <= 0 || L[2] <= 0)
		{
			pdf = 0;
			return 0;
		}

		pdf = L[2] / 3.14159f;

		float NdotV = V[2];
        float NdotL = L[2];
		float LdotH = Dot(L, Normalize(V+L));
		float perceptualRoughness = sqrtf(alpha);
		float fd90 = 0.5f + 2 * LdotH * LdotH * perceptualRoughness;
		float lightScatter    = (1 + (fd90 - 1) * powf(1 - NdotL, 5.0f));
		float viewScatter    = (1 + (fd90 - 1) * powf(1 - NdotV, 5.0f));
        return lightScatter * viewScatter * L[2] / 3.14159f;
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
