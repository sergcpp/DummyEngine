#include "Fence.h"

#include <cassert>
#include <utility>

namespace Ren {
#ifndef REN_EXCHANGE_DEFINED
template <class T, class U = T> T exchange(T &obj, U &&new_value) {
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}
#define REN_EXCHANGE_DEFINED
#endif
}

Ren::SyncFence::~SyncFence() {
    if (fence_) {
        vkDestroyFence(device_, fence_, nullptr);
    }
}

Ren::SyncFence::SyncFence(SyncFence &&rhs) {
    device_ = exchange(rhs.device_, VkDevice{VK_NULL_HANDLE});
    fence_ = exchange(rhs.fence_, VkFence{VK_NULL_HANDLE});
}

Ren::SyncFence &Ren::SyncFence::operator=(SyncFence &&rhs) {
    if (fence_) {
        vkDestroyFence(device_, fence_, nullptr);
    }
    device_ = exchange(rhs.device_, VkDevice{VK_NULL_HANDLE});
    fence_ = exchange(rhs.fence_, VkFence{VK_NULL_HANDLE});
    return (*this);
}

bool Ren::SyncFence::Reset() {
    const VkResult res = vkResetFences(device_, 1, &fence_);
    return res == VK_SUCCESS;
}

void Ren::SyncFence::WaitSync() {
    // assert(sync_);
    // glWaitSync(reinterpret_cast<GLsync>(sync_), 0, GL_TIMEOUT_IGNORED);
}

Ren::WaitResult Ren::SyncFence::ClientWaitSync(const uint64_t timeout_us) {
    assert(fence_ != VK_NULL_HANDLE);
    const VkResult res = vkWaitForFences(device_, 1, &fence_, VK_TRUE, timeout_us * 1000);

    WaitResult ret = WaitResult::Fail;
    if (res == VK_TIMEOUT) {
        ret = WaitResult::Timeout;
    } else if (res == VK_SUCCESS) {
        ret = WaitResult::Success;
    }

    return ret;
}

Ren::SyncFence Ren::MakeFence() { return SyncFence{VK_NULL_HANDLE, VK_NULL_HANDLE}; }
