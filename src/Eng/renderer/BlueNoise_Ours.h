#pragma once

//
// Time-coupled 3D blue noise texture generator. Follows the ideas from:
// https://hal.science/hal-02150657, https://doi.org/10.2312/sr.20221161, https://arxiv.org/pdf/2112.09629, https://arxiv.org/pdf/2310.15364
//

namespace Eng {
// The filter applied to XY dimensions
enum class eSpatialFilter { Gauss };

// The filter applied to Z dimension
enum class eTemporalFilter {
    Gauss,
    TruncatedEMA, // exponential moving average with random history rejection
    TruncatedLinear
};

//
// All functions produce 64x64 tiles of SampleCount samples
//

// "Void and Cluster" method
// + excellent BN quality
// + fast (a couple of minutes)
// - only suitable for 1D use case
// - doesn't preserve XY-slice histogram (stratification is lost)
// - no way to choose test function (maximizes sample distance, equivalent to step function error optimization)
template <int Log2SampleCount, eSpatialFilter sf = eSpatialFilter::Gauss,
          eTemporalFilter tf = eTemporalFilter::TruncatedEMA>
void Generate1D_TCBN_VC(unsigned int seed, bool strided_access);

// Pixel swap method
// + very simple to implement
// + preserves XY-slice histogram (stratification is kept)
// + flexible to test function choice (doesn't make much sense for 1D case however)
// - poor BN quality (cannot reach V&C, always gets stuck in local maximum)
// - very slow (few hours on a single thread)
template <int Log2SampleCount, eSpatialFilter sf = eSpatialFilter::Gauss,
          eTemporalFilter tf = eTemporalFilter::TruncatedEMA>
void Generate1D_TCBN_Swap(unsigned int seed);

template <int Log2SampleCount, eSpatialFilter sf = eSpatialFilter::Gauss,
          eTemporalFilter tf = eTemporalFilter::TruncatedEMA>
void Generate2D_TCBN(unsigned int seed);
} // namespace Eng