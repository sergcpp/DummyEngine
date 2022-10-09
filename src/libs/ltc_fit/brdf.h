#ifndef _BRDF_
#define _BRDF_

#include "MVec.h"

class Brdf
{
public:
	// evaluation of the cosine-weighted BRDF
	// pdf is set to the PDF of sampling L
	virtual float eval(const Vec3f& V, const Vec3f& L, const float alpha, float& pdf) const = 0;

	// sampling
	virtual Vec3f sample(const Vec3f& V, const float alpha, const float U1, const float U2) const = 0;
};


#endif
