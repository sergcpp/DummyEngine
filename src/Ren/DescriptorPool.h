#pragma once

#include "VK.h"

namespace Ren {
struct ApiContext;

//
// DescrPool is able to allocate up to fixed amount of sets of specific size
//
enum class eDescrType : uint8_t {
    CombinedImageSampler,
    StorageImage,
    UniformBuffer,
    StorageBuffer,
    UniformTexBuffer,
    AccStructure,
    _Count
};

struct DescrSizes {
    uint32_t img_sampler_count = 0;
    uint32_t store_img_count = 0;
    uint32_t ubuf_count = 0;
    uint32_t sbuf_count = 0;
    uint32_t tbuf_count = 0;
    uint32_t acc_count = 0;
};

class DescrPool {
    ApiContext *api_ctx_ = nullptr;
    VkDescriptorPool handle_ = VK_NULL_HANDLE;
    uint32_t sets_count_ = 0, next_free_ = 0;

    uint32_t descr_counts_[int(eDescrType::_Count)] = {};

  public:
    DescrPool(ApiContext *api_ctx) : api_ctx_(api_ctx) {}
    DescrPool(const DescrPool &rhs) = delete;
    DescrPool(DescrPool &&rhs) noexcept { (*this) = std::move(rhs); }
    ~DescrPool() { Destroy(); }

    DescrPool &operator=(const DescrPool &rhs) = delete;
    DescrPool &operator=(DescrPool &&rhs) noexcept;

    ApiContext *api_ctx() { return api_ctx_; }

    uint32_t free_count() const { return sets_count_ - next_free_; }
    uint32_t descr_count(const eDescrType type) const { return descr_counts_[int(type)]; }

    bool Init(const DescrSizes &sizes, uint32_t sets_count);
    void Destroy();

    VkDescriptorSet Alloc(VkDescriptorSetLayout layout);
    bool Reset();
};

//
// DescrPoolAlloc is able to allocate any amount of sets of specific size
//
class DescrPoolAlloc {
    ApiContext *api_ctx_ = nullptr;
    DescrSizes sizes_;
    uint32_t initial_sets_count_ = 0;

    SmallVector<DescrPool, 256> pools_;
    int next_free_pool_ = -1;

  public:
    DescrPoolAlloc(ApiContext *api_ctx, const DescrSizes &sizes, const uint32_t initial_sets_count)
        : api_ctx_(api_ctx), sizes_(sizes), initial_sets_count_(initial_sets_count) {}

    ApiContext *api_ctx() { return api_ctx_; }

    VkDescriptorSet Alloc(VkDescriptorSetLayout layout);
    bool Reset();
};

//
// DescrMultiPoolAlloc is able to allocate any amount of sets of any size
//
class DescrMultiPoolAlloc {
    uint32_t pool_step_ = 0;
    uint32_t img_sampler_based_count_ = 0, store_img_based_count_ = 0, ubuf_based_count_ = 0, sbuf_based_count_ = 0,
             tbuf_based_count_ = 0, acc_based_count_;
    uint32_t max_img_sampler_count_ = 0, max_store_img_count_ = 0, max_ubuf_count_ = 0, max_sbuf_count_ = 0,
             max_tbuf_count_ = 0, max_acc_count_ = 0;
    SmallVector<DescrPoolAlloc, 16> pools_;

  public:
    DescrMultiPoolAlloc(ApiContext *api_ctx, uint32_t pool_step, uint32_t max_img_sampler_count,
                        uint32_t max_store_img_count, uint32_t max_ubuf_count, uint32_t max_sbuf_count,
                        uint32_t max_tbuf_count, uint32_t max_acc_count, uint32_t initial_sets_count);

    ApiContext *api_ctx() { return pools_.front().api_ctx(); }

    VkDescriptorSet Alloc(const DescrSizes &sizes, VkDescriptorSetLayout layout);
    bool Reset();
};
} // namespace Ren