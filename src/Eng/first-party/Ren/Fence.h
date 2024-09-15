#pragma once

#include <cstdint>

#include "Fwd.h"

namespace Ren {
struct ApiContext;
enum class WaitResult { Success, Timeout, Fail };
class SyncFence {
#if defined(USE_VK_RENDER)
    ApiContext *api_ctx_ = nullptr;
    VkFence fence_ = {};
#elif defined(USE_GL_RENDER)
    void *sync_ = nullptr;
#endif

  public:
    SyncFence() = default;
#if defined(USE_VK_RENDER)
    SyncFence(ApiContext *api_ctx, VkFence fence) : api_ctx_(api_ctx), fence_(fence) {}
#elif defined(USE_GL_RENDER)
    SyncFence(void *sync) : sync_(sync) {}
#endif
    ~SyncFence();

    SyncFence(const SyncFence &rhs) = delete;
    SyncFence(SyncFence &&rhs) noexcept;
    SyncFence &operator=(const SyncFence &rhs) = delete;
    SyncFence &operator=(SyncFence &&rhs) noexcept;

#if defined(USE_VK_RENDER)
    operator bool() const { return fence_ != VkFence{}; }
    [[nodiscard]] VkFence fence() const { return fence_; }

    [[nodiscard]] bool signaled() const;

    bool Reset();
#elif defined(USE_GL_RENDER)
    operator bool() const { return sync_ != nullptr; }
#endif

    void WaitSync();
    WaitResult ClientWaitSync(uint64_t timeout_us = 1000000000);
};

SyncFence MakeFence();
}