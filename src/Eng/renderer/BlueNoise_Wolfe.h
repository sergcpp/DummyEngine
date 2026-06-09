#pragma once

#include <Ren/math/Vec.h>

//
// Blue noise generator described in "Spatiotemporal Blue Noise Masks" and "Scalar Spatiotemporal Blue Noise Masks"
// https://doi.org/10.2312/sr.20221161, https://arxiv.org/pdf/2112.09629
//

namespace Eng {
//
// Both functions produce 64x64 tiles of SampleCount samples
//

// "Void and Cluster" modified to include time dimension
template <int Log2SampleCount> void Generate1D_STBN(unsigned int seed, bool strided_access);

// Swap algorithrm
template <int Log2SampleCount> void Generate2D_STBN(unsigned int seed);
}