#pragma once

#include <cstdint>

//
// Blue noise optimization described in "A Low-Discrepancy Sampler that Distributes Monte Carlo Errors as a Blue Noise
// in Screen Space" https://hal.science/hal-02150657
//

namespace Eng {
// This is a slow single-threaded implementation, but it's ok for now
// Initial samples must be 24-bit fixed-point numbers obtained with 'uint32_t(val * 16777216.0) << 8'
void Generate1D_BlueNoiseTiles_StepFunction(const uint32_t initial_samples[]);

/*
#include <Ray/internal/PMJ.h>
#include <Ren/Span.h>
Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(123456, 256, 1);
uint32_t initial_samples[256];
for (int i = 0; i < 256; ++i) {
    initial_samples[i] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
}
Eng::Generate1D_BlueNoiseTiles_StepFunction(initial_samples);
*/
} // namespace Eng