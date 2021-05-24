#pragma once

namespace Ren {
class Sampler : public RefCounter {
    uint32_t id_ = 0;
    SamplingParams params_;

    void Destroy();

  public:
    Sampler() = default;
    Sampler(const Sampler &rhs) = delete;
    Sampler(Sampler &&rhs);
    ~Sampler() { Destroy(); }

    SamplingParams params() const { return params_; }

    Sampler &operator=(const Sampler &rhs) = delete;
    Sampler &operator=(Sampler &&rhs);

    void Init(SamplingParams params);
};
} // namespace Ren