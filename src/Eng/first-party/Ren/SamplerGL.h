#pragma once

#include "SamplingParams.h"
#include "Storage.h"

namespace Ren {
struct ApiContext;
class Sampler : public RefCounter {
    uint32_t id_ = 0;
    SamplingParamsPacked params_;

    void Destroy();

  public:
    Sampler() = default;
    Sampler(const Sampler &rhs) = delete;
    Sampler(Sampler &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Sampler() { Destroy(); }

    uint32_t id() const { return id_; }
    SamplingParams params() const { return params_; }

    Sampler &operator=(const Sampler &rhs) = delete;
    Sampler &operator=(Sampler &&rhs);

    void Init(ApiContext *api_ctx, SamplingParams params);
};

void GLUnbindSamplers(int start, int count);
} // namespace Ren