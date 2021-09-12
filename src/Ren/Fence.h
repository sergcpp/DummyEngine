#pragma once

#include <cstdint>

#if defined(USE_VK_RENDER)
#include "VK.h"
#endif

namespace Ren {
enum class WaitResult { Success, Timeout, Fail };

class SyncFence {
#if defined(USE_VK_RENDER)
    VkDevice device_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;
#elif defined(USE_GL_RENDER)
    void *sync_ = nullptr;
#endif

  public:
    SyncFence() = default;
#if defined(USE_VK_RENDER)
    SyncFence(VkDevice device, VkFence fence) : device_(device), fence_(fence) {}
#elif defined(USE_GL_RENDER)
    SyncFence(void *sync) : sync_(sync) {}
#endif
    ~SyncFence();

    SyncFence(const SyncFence &rhs) = delete;
    SyncFence(SyncFence &&rhs);
    SyncFence &operator=(const SyncFence &rhs) = delete;
    SyncFence &operator=(SyncFence &&rhs);

#if defined(USE_VK_RENDER)
    operator bool() const { return fence_ != VK_NULL_HANDLE; }
    VkFence fence() { return fence_; }

    bool signaled() const { return vkGetFenceStatus(device_, fence_) == VK_SUCCESS; }

    bool Reset();
#elif defined(USE_GL_RENDER)
    operator bool() const { return sync_ != nullptr; }
#endif

    void WaitSync();
    WaitResult ClientWaitSync(uint64_t timeout_us = 1000000000);
};

SyncFence MakeFence();
}