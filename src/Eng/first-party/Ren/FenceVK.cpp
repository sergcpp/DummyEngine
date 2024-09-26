#include "Fence.h"

#include <cassert>
#include <utility>

#include "VKCtx.h"

Ren::SyncFence::~SyncFence() {
    if (fence_) {
        api_ctx_->vkDestroyFence(api_ctx_->device, fence_, nullptr);
    }
}

Ren::SyncFence::SyncFence(SyncFence &&rhs) noexcept {
    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    fence_ = std::exchange(rhs.fence_, VkFence{VK_NULL_HANDLE});
}

bool Ren::SyncFence::signaled() const { return api_ctx_->vkGetFenceStatus(api_ctx_->device, fence_) == VK_SUCCESS; }

Ren::SyncFence &Ren::SyncFence::operator=(SyncFence &&rhs) noexcept {
    if (fence_) {
        api_ctx_->vkDestroyFence(api_ctx_->device, fence_, nullptr);
    }
    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    fence_ = std::exchange(rhs.fence_, VkFence{VK_NULL_HANDLE});
    return (*this);
}

bool Ren::SyncFence::Reset() {
    const VkResult res = api_ctx_->vkResetFences(api_ctx_->device, 1, &fence_);
    return res == VK_SUCCESS;
}

void Ren::SyncFence::WaitSync() {
    // assert(sync_);
    // glWaitSync(reinterpret_cast<GLsync>(sync_), 0, GL_TIMEOUT_IGNORED);
}

Ren::eWaitResult Ren::SyncFence::ClientWaitSync(const uint64_t timeout_us) {
    assert(fence_ != VK_NULL_HANDLE);
    const VkResult res = api_ctx_->vkWaitForFences(api_ctx_->device, 1, &fence_, VK_TRUE, timeout_us * 1000);

    eWaitResult ret = eWaitResult::Fail;
    if (res == VK_TIMEOUT) {
        ret = eWaitResult::Timeout;
    } else if (res == VK_SUCCESS) {
        ret = eWaitResult::Success;
    }

    return ret;
}

Ren::SyncFence Ren::MakeFence() { return SyncFence{VK_NULL_HANDLE, VK_NULL_HANDLE}; }
