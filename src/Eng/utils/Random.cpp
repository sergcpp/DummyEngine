#include "Random.h"

#include <random>

namespace Eng {
class Random_Impl {
  public:
    std::mt19937 gen;
    std::uniform_real_distribution<float> n_float_distr;
    std::uniform_real_distribution<float> minus_1_to_1_float_distr;
    std::uniform_real_distribution<float> zero_to_2pi_float_distr;
    std::uniform_real_distribution<double> n_double_distr;

    explicit Random_Impl(uint32_t seed)
        : gen(seed), n_float_distr{0, 1}, minus_1_to_1_float_distr{-1, 1}, zero_to_2pi_float_distr{0.0f, 6.2831853f},
          n_double_distr{0, 1} {}
};
} // namespace Eng

Eng::Random::Random(uint32_t seed) {
    impl_ = std::make_unique<Random_Impl>(seed);
}

Eng::Random::~Random() = default;

int Eng::Random::GetInt(int min, int max) {
    return std::uniform_int_distribution<int> { min, max }(impl_->gen);
}

float Eng::Random::GetFloat(float min, float max) {
    return std::uniform_real_distribution<float> { min, max }(impl_->gen);
}

float Eng::Random::GetNormalizedFloat() {
    return impl_->n_float_distr(impl_->gen);
}

float Eng::Random::GetMinus1to1Float() {
    return impl_->minus_1_to_1_float_distr(impl_->gen);
}

Ren::Vec3f Eng::Random::GetUnitVec3() {
    const float
        omega = impl_->zero_to_2pi_float_distr(impl_->gen),
        z = impl_->minus_1_to_1_float_distr(impl_->gen);

    return Ren::Vec3f{
        std::sqrt(1.0f - z * z) * std::cos(omega), std::sqrt(1.0f - z * z) * std::sin(omega), z
    };
}

double Eng::Random::GetNormalizedDouble() {
    return impl_->n_double_distr(impl_->gen);
}