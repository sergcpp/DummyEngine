#pragma once

#include <memory>

#include <Ren/MVec.h>

namespace Eng {
class Random_Impl;
class Random {
    std::unique_ptr<Random_Impl> impl_;

  public:
    explicit Random(uint32_t seed);
    ~Random();

    int GetInt(int min, int max);
    float GetFloat(float min, float max);
    float GetNormalizedFloat();
    float GetMinus1to1Float();
    Ren::Vec3f GetUnitVec3();

    double GetNormalizedDouble();
};
} // namespace Eng