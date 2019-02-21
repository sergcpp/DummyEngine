#include "Random.h"

#include <random>

class Random_Impl {
public:
    std::mt19937 gen;
    std::uniform_real_distribution<float> n_float_distr;
    std::uniform_real_distribution<float> minus_1_to_1_float_distr;
    std::uniform_real_distribution<double> n_double_distr;

    explicit Random_Impl(uint32_t seed)
        : gen(seed), n_float_distr{ 0, 1 }, minus_1_to_1_float_distr{ -1, 1 },
          n_double_distr{ 0, 1 } {}
};

Random::Random(uint32_t seed) {
    impl_.reset(new Random_Impl(seed));
}

int Random::GetInt(int min, int max) {
    return std::uniform_int_distribution<int> { min, max }(impl_->gen);
}

float Random::GetFloat(float min, float max) {
    return std::uniform_real_distribution<float> { min, max }(impl_->gen);
}

float Random::GetNormalizedFloat() {
    return impl_->n_float_distr(impl_->gen);
}

float Random::GetMinus1to1Float() {
    return impl_->minus_1_to_1_float_distr(impl_->gen);
}

Ren::Vec3f Random::GetNormalizedVec3() {
    return Ren::Normalize( Ren::Vec3f{
        impl_->minus_1_to_1_float_distr(impl_->gen),
        impl_->minus_1_to_1_float_distr(impl_->gen),
        impl_->minus_1_to_1_float_distr(impl_->gen)
    });
}

double Random::GetNormalizedDouble() {
    return impl_->n_double_distr(impl_->gen);
}