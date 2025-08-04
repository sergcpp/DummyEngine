#pragma once

#include <Ren/MVec.h>

//
// Blue noise generator described in "Scalar Spatiotemporal Blue Noise Masks" https://arxiv.org/pdf/2112.09629
//

namespace Eng {
template <int Log2SampleCount> void Generate1D_STBN();
}