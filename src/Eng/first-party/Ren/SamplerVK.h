#pragma once

#include "SamplingParams.h"
#include "Storage.h"
#include "VK.h"

namespace Ren {
struct ApiContext;
class Sampler : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    VkSampler handle_ = VK_NULL_HANDLE;
    SamplingParamsPacked params_;

  public:
    Sampler() = default;
    Sampler(const Sampler &rhs) = delete;
    Sampler(Sampler &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Sampler() { Destroy(); }

    VkSampler vk_handle() const { return handle_; }
    SamplingParams params() const { return params_; }

    operator bool() const { return handle_ != VK_NULL_HANDLE; }

    Sampler &operator=(const Sampler &rhs) = delete;
    Sampler &operator=(Sampler &&rhs) noexcept;

    void Init(ApiContext *api_ctx, SamplingParams params);
    void Destroy();
};

} // namespace Ren