#pragma once

#include <utility>

#include "Fwd.h"
#include "utils/SmallVector.h"

namespace Ren {
struct ApiContext;

enum class eDescrType : uint8_t {
    CombinedImageSampler,
    SampledImage,
    Sampler,
    StorageImage,
    UniformBuffer,
    UniformTexBuffer,
    StorageBuffer,
    StorageTexBuffer,
    AccStructure,
    _Count
};

struct DescrSizes {
    uint32_t img_sampler_count = 0;
    uint32_t img_count = 0;
    uint32_t sampler_count = 0;
    uint32_t store_img_count = 0;
    uint32_t ubuf_count = 0;
    uint32_t utbuf_count = 0;
    uint32_t sbuf_count = 0;
    uint32_t stbuf_count = 0;
    uint32_t acc_count = 0;
};

//
// DescrPool is able to allocate up to fixed amount of sets of specific size
//
class DescrPool {
    const ApiContext &api_;
#if defined(REN_VK_BACKEND)
    VkDescriptorPool handle_ = {};
#endif
    uint32_t sets_count_ = 0, next_free_ = 0;

    uint32_t descr_counts_[int(eDescrType::_Count)] = {};

  public:
    DescrPool(const ApiContext &api) : api_(api) {}
    DescrPool(const DescrPool &rhs) = delete;
    DescrPool(DescrPool &&rhs) noexcept : api_(rhs.api_) { (*this) = std::move(rhs); }
    ~DescrPool() { Destroy(); }

    DescrPool &operator=(const DescrPool &rhs) = delete;
    DescrPool &operator=(DescrPool &&rhs) noexcept;

    [[nodiscard]] const ApiContext &api() { return api_; }

    [[nodiscard]] uint32_t free_count() const { return sets_count_ - next_free_; }
    [[nodiscard]] uint32_t descr_count(const eDescrType type) const { return descr_counts_[int(type)]; }

    [[nodiscard]] bool Init(const DescrSizes &sizes, uint32_t sets_count);
    void Destroy();

#if defined(REN_VK_BACKEND)
    [[nodiscard]] VkDescriptorSet Alloc(VkDescriptorSetLayout layout);
    [[nodiscard]] bool Reset();
#endif
};

//
// DescrPoolAlloc is able to allocate any amount of sets of specific size
//
class DescrPoolAlloc {
    const ApiContext &api_;
    DescrSizes sizes_;
    uint32_t initial_sets_count_ = 0;

    SmallVector<DescrPool, 256> pools_;
    int next_free_pool_ = -1;

  public:
    DescrPoolAlloc(const ApiContext &api, const DescrSizes &sizes, const uint32_t initial_sets_count)
        : api_(api), sizes_(sizes), initial_sets_count_(initial_sets_count) {}

    [[nodiscard]] const ApiContext &api() { return api_; }

#if defined(REN_VK_BACKEND)
    [[nodiscard]] VkDescriptorSet Alloc(VkDescriptorSetLayout layout);
    [[nodiscard]] bool Reset();
#endif
};

//
// DescrMultiPoolAlloc is able to allocate any amount of sets of any size
//
class DescrMultiPoolAlloc {
    uint32_t pool_step_ = 0;
    uint32_t img_sampler_based_count_ = 0, img_based_count_ = 0, sampler_based_count_ = 0, store_img_based_count_ = 0,
             ubuf_based_count_ = 0, utbuf_based_count_ = 0, sbuf_based_count_ = 0, stbuf_based_count_ = 0,
             acc_based_count_ = 0;
    uint32_t max_img_sampler_count_ = 0, max_img_count_ = 0, max_sampler_count_ = 0, max_store_img_count_ = 0,
             max_ubuf_count_ = 0, max_utbuf_count_ = 0, max_sbuf_count_ = 0, max_stbuf_count_ = 0, max_acc_count_ = 0;
    SmallVector<DescrPoolAlloc, 16> pools_;

  public:
    DescrMultiPoolAlloc(const ApiContext &api, uint32_t pool_step, uint32_t max_img_sampler_count,
                        uint32_t max_img_count, uint32_t max_sampler_count, uint32_t max_store_img_count,
                        uint32_t max_ubuf_count, uint32_t max_utbuf_count, uint32_t max_sbuf_count,
                        uint32_t max_stbuf_count, uint32_t max_acc_count, uint32_t initial_sets_count);

    [[nodiscard]] const ApiContext &api() { return pools_.front().api(); }

#if defined(REN_VK_BACKEND)
    [[nodiscard]] VkDescriptorSet Alloc(const DescrSizes &sizes, VkDescriptorSetLayout layout);
    [[nodiscard]] bool Reset();
#endif
};
} // namespace Ren