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

	// Optional single-scatter energy terms baked into the amplitude LUT
	// (tex2.y = E(N_dot_V, sigma), tex2.z = Eavg(sigma)). Used to evaluate a
	// Kulla-Conty multi-scatter compensation at runtime without any trig.
	virtual bool supports_ltc_energy() const { return false; }
	virtual float compute_E(const float nv, const float sigma) const { return 1.0f; }
	virtual float compute_Eavg(const float sigma) const { return 1.0f; }
};


#endif
