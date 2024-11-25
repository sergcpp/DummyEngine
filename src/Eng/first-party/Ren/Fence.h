#pragma once

#include <cstdint>

#include "Fwd.h"

namespace Ren {
struct ApiContext;
enum class eWaitResult { Success, Timeout, Fail };
class SyncFence {
#if defined(REN_VK_BACKEND)
    ApiContext *api_ctx_ = nullptr;
    VkFence fence_ = {};
#elif defined(REN_GL_BACKEND)
    void *sync_ = nullptr;
#endif

  public:
    SyncFence() = default;
#if defined(REN_VK_BACKEND)
    SyncFence(ApiContext *api_ctx, VkFence fence) : api_ctx_(api_ctx), fence_(fence) {}
#elif defined(REN_GL_BACKEND)
    SyncFence(void *sync) : sync_(sync) {}
#endif
    ~SyncFence();

    SyncFence(const SyncFence &rhs) = delete;
    SyncFence(SyncFence &&rhs) noexcept;
    SyncFence &operator=(const SyncFence &rhs) = delete;
    SyncFence &operator=(SyncFence &&rhs) noexcept;

#if defined(REN_VK_BACKEND)
    operator bool() const { return fence_ != VkFence{}; }
    [[nodiscard]] VkFence fence() const { return fence_; }

    [[nodiscard]] bool signaled() const;

    bool Reset();
#elif defined(REN_GL_BACKEND)
    operator bool() const { return sync_ != nullptr; }
#endif

    void WaitSync();
    eWaitResult ClientWaitSync(uint64_t timeout_us = 1000000000);
};

SyncFence MakeFence();
}
