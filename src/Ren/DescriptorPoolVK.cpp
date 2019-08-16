#include "DescriptorPool.h"

#include "SmallVector.h"
#include "VKCtx.h"

namespace Ren {
const VkDescriptorType g_descr_types_vk[] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                             VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER};
static_assert(COUNT_OF(g_descr_types_vk) == int(eDescrType::_Count), "!");
} // namespace Ren

Ren::DescrPool &Ren::DescrPool::operator=(DescrPool &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Destroy();

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, {});
    sets_count_ = exchange(rhs.sets_count_, 0);
    next_free_ = exchange(rhs.next_free_, 0);
    for (int i = 0; i < int(eDescrType::_Count); ++i) {
        descr_counts_[i] = rhs.descr_counts_[i];
    }

    return (*this);
}

bool Ren::DescrPool::Init(const uint32_t img_count, const uint32_t ubuf_count, const uint32_t sbuf_count,
                          const uint32_t tbuf_count, const uint32_t sets_count) {
    Destroy();

    descr_counts_[int(eDescrType::CombinedImageSampler)] = img_count;
    descr_counts_[int(eDescrType::UniformBuffer)] = ubuf_count;
    descr_counts_[int(eDescrType::StorageBuffer)] = sbuf_count;
    descr_counts_[int(eDescrType::UniformTexBuffer)] = tbuf_count;

    SmallVector<VkDescriptorPoolSize, int(eDescrType::_Count)> pool_sizes;
    for (int i = 0; i < int(eDescrType::_Count); ++i) {
        if (descr_counts_[i]) {
            pool_sizes.push_back({g_descr_types_vk[i], sets_count * descr_counts_[i]});
        }
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = uint32_t(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.cdata();
    pool_info.maxSets = sets_count;

    sets_count_ = sets_count;

    const VkResult res = vkCreateDescriptorPool(api_ctx_->device, &pool_info, nullptr, &handle_);
    return res == VK_SUCCESS;
}

void Ren::DescrPool::Destroy() {
    if (handle_) {
        api_ctx_->descriptor_pools_to_destroy[api_ctx_->backend_frame].emplace_back(handle_);
        handle_ = {};
    }
}

VkDescriptorSet Ren::DescrPool::Alloc(const VkDescriptorSetLayout layout) {
    if (next_free_ >= sets_count_) {
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = handle_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet descr_set = VK_NULL_HANDLE;
    const VkResult res = vkAllocateDescriptorSets(api_ctx_->device, &alloc_info, &descr_set);
    assert(res == VK_SUCCESS);

    ++next_free_;

    return descr_set;
}

bool Ren::DescrPool::Reset() {
    next_free_ = 0;
    const VkResult res = vkResetDescriptorPool(api_ctx_->device, handle_, 0);
    return res == VK_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

VkDescriptorSet Ren::DescrPoolAlloc::Alloc(const VkDescriptorSetLayout layout) {
    if (next_free_pool_ == -1 || pools_[next_free_pool_].free_count() == 0) {
        ++next_free_pool_;

        if (next_free_pool_ == pools_.size()) {
            // allocate twice more sets each time
            const uint32_t count_mul = (1u << pools_.size());

            DescrPool &new_pool = pools_.emplace_back(api_ctx_);
            if (!new_pool.Init(img_count_, ubuf_count_, sbuf_count_, tbuf_count_, count_mul * initial_sets_count_)) {
                return VK_NULL_HANDLE;
            }
        }
    }
    return pools_[next_free_pool_].Alloc(layout);
}

bool Ren::DescrPoolAlloc::Reset() {
    if (pools_.empty()) {
        return true;
    }

    bool result = true;
    for (auto &pool : pools_) {
        result &= pool.Reset();
    }
    next_free_pool_ = 0;
    return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

Ren::DescrMultiPoolAlloc::DescrMultiPoolAlloc(ApiContext *api_ctx, const uint32_t pool_step,
                                              const uint32_t max_img_count, const uint32_t max_ubuf_count,
                                              const uint32_t max_sbuf_count, const uint32_t max_tbuf_count,
                                              const uint32_t initial_sets_count)
    : pool_step_(pool_step) {
    img_based_count_ = (max_img_count + pool_step - 1) / pool_step;
    ubuf_based_count_ = (max_ubuf_count + pool_step - 1) / pool_step;
    sbuf_based_count_ = (max_sbuf_count + pool_step - 1) / pool_step;
    tbuf_based_count_ = (max_tbuf_count + pool_step - 1) / pool_step;
    const uint32_t required_pools_count = img_based_count_ * ubuf_based_count_ * sbuf_based_count_ * tbuf_based_count_;

    // store rounded values
    max_img_count_ = pool_step * img_based_count_;
    max_ubuf_count_ = pool_step * ubuf_based_count_;
    max_sbuf_count_ = pool_step * sbuf_based_count_;
    max_tbuf_count_ = pool_step * tbuf_based_count_;

    for (uint32_t i = 0; i < required_pools_count; ++i) {
        uint32_t index = i;

        const uint32_t tbuf_count = pool_step * ((index % tbuf_based_count_) + 1);
        index /= tbuf_based_count_;
        const uint32_t sbuf_count = pool_step * ((index % sbuf_based_count_) + 1);
        index /= sbuf_based_count_;
        const uint32_t ubuf_count = pool_step * ((index % ubuf_based_count_) + 1);
        index /= ubuf_based_count_;
        const uint32_t img_count = pool_step * ((index % img_based_count_) + 1);
        index /= img_based_count_;

        pools_.emplace_back(api_ctx, img_count, ubuf_count, sbuf_count, tbuf_count, initial_sets_count);
    }
    assert(pools_.size() == required_pools_count);
}

VkDescriptorSet Ren::DescrMultiPoolAlloc::Alloc(const uint32_t img_count, const uint32_t ubuf_count,
                                                const uint32_t sbuf_count, const uint32_t tbuf_count,
                                                const VkDescriptorSetLayout layout) {
    const uint32_t img_based_index = img_count ? ((img_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t ubuf_based_index = ubuf_count ? ((ubuf_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t sbuf_based_index = sbuf_count ? ((sbuf_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t tbuf_based_index = tbuf_count ? ((tbuf_count + pool_step_ - 1) / pool_step_ - 1) : 0;

    const uint32_t pool_index = img_based_index * ubuf_based_count_ * sbuf_based_count_ * tbuf_based_count_ +
                                ubuf_based_index * sbuf_based_count_ * tbuf_based_count_ +
                                sbuf_based_index * tbuf_based_count_ + tbuf_based_index;
    return pools_[pool_index].Alloc(layout);
}

bool Ren::DescrMultiPoolAlloc::Reset() {
    bool result = true;
    for (auto &pool : pools_) {
        result &= pool.Reset();
    }
    return result;
}