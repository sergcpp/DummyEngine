#pragma once

#include <cstdint>

#include <Ren/MVec.h>

//
// Blue noise optimization described in "A Low-Discrepancy Sampler that Distributes Monte Carlo Errors as a Blue Noise
// in Screen Space" https://hal.science/hal-02150657
//

namespace Eng {
/*
Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(123456, 256, 1);
uint32_t initial_samples[256];
for (int i = 0; i < 256; ++i) {
    initial_samples[i] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
}
Eng::Generate1D_BlueNoiseTiles_StepFunction(initial_samples);
*/

// Initial samples must be 24-bit fixed-point numbers obtained with 'uint32_t(val * 16777216.0) << 8'
void Generate1D_BlueNoiseTiles_StepFunction(const uint32_t initial_samples[]);

/*{ // Dims 0, 1
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0xdf2c196c, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(0, initial_samples, 0x97c083e3);
}
{ // Dims 2, 3
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x9fcc6472, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(1, initial_samples, 0xe7955b1e);
}
{ // Dims 4, 5
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x34246120, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(2, initial_samples, 0xb3e00166);
}
{ // Dims 6, 7
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x093118a8, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(3, initial_samples, 0x5f047a2c);
}
{ // Dims 8, 9
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x15ecd441, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(4, initial_samples, 0xf6a26ff4);
}
{ // Dims 10, 11
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x1e313309, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(5, initial_samples, 0x786a5d4d);
}
{ // Dims 12, 13
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0xf53a4eb9, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(6, initial_samples, 0x10ccee0f);
}
{ // Dims 14, 15
    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x5d54efe3, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(7, initial_samples, 0xce15c1af);
}*/

// Initial samples must be 24-bit fixed-point numbers obtained with 'uint32_t(val * 16777216.0) << 8'
void Generate2D_BlueNoiseTiles_StepFunction(int index, const Ren::Vec2u initial_samples[], uint32_t seed);
} // namespace Eng