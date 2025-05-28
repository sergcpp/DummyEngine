#include "DummyApp.h"

#include <Eng/renderer/BlueNoise.h>
#include <Ray/internal/PMJ.h>

int main(int argc, char *argv[]) {
    /*{ // Dims 0, 1
        Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0xdf2c196c, 256, 1);
        Ren::Vec2u initial_samples[256];
        for (int i = 0; i < 256; ++i) {
            initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
            initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
        }
        Eng::Generate2D_BlueNoiseTiles_StepFunction(initial_samples, 0x97c083e3);
    }*/
    { // Dims 2, 3
        Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x9fcc6472, 256, 1);
        Ren::Vec2u initial_samples[256];
        for (int i = 0; i < 256; ++i) {
            initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
            initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
        }
        Eng::Generate2D_BlueNoiseTiles_StepFunction(initial_samples, 0xe7955b1e);
    }
    /*{ // Dims 4, 5
        Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x34246120, 256, 1);
        Ren::Vec2u initial_samples[256];
        for (int i = 0; i < 256; ++i) {
            initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
            initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
        }
        Eng::Generate2D_BlueNoiseTiles_StepFunction(initial_samples, 0xb3e00166);
    }
    { // Dims 6, 7
        Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(0x093118a8, 256, 1);
        Ren::Vec2u initial_samples[256];
        for (int i = 0; i < 256; ++i) {
            initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
            initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
        }
        Eng::Generate2D_BlueNoiseTiles_StepFunction(initial_samples, 0x5f047a2c);
    }*/
    return 0;

    return DummyApp().Run(argc, argv);
}
