#include "DummyApp.h"

#include <Eng/renderer/BlueNoise.h>
#include <Ray/internal/PMJ.h>

int main(int argc, char *argv[]) {

    Ray::aligned_vector<Ray::Ref::dvec2> pmj_samples = Ray::GeneratePMJSamples(65412, 256, 1);
    Ren::Vec2u initial_samples[256];
    for (int i = 0; i < 256; ++i) {
        initial_samples[i][0] = uint32_t(pmj_samples[i].get<0>() * 16777216.0) << 8;
        initial_samples[i][1] = uint32_t(pmj_samples[i].get<1>() * 16777216.0) << 8;
    }
    Eng::Generate2D_BlueNoiseTiles_StepFunction(initial_samples);
    return 0;

    return DummyApp().Run(argc, argv);
}
