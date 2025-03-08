#include "DescriptorPool.h"

#include "SmallVector.h"
#include "VKCtx.h"

namespace Ren {
const VkDescriptorType g_descr_types_vk[] = {
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    // CombinedImageSampler
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,             // SampledImage
    VK_DESCRIPTOR_TYPE_SAMPLER,                   // Sampler
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             // StorageImage
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            // UniformBuffer
    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,      // UniformTexBuffer
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            // StorageBuffer
    VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,      // StorageTexBuffer
    VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR // AccStructure
};
static_assert(std::size(g_descr_types_vk) == int(eDescrType::_Count), "!");
} // namespace Ren

Ren::DescrPool &Ren::DescrPool::operator=(DescrPool &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Destroy();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    sets_count_ = std::exchange(rhs.sets_count_, 0);
    next_free_ = std::exchange(rhs.next_free_, 0);
    for (int i = 0; i < int(eDescrType::_Count); ++i) {
        descr_counts_[i] = rhs.descr_counts_[i];
    }

    return (*this);
}

bool Ren::DescrPool::Init(const DescrSizes &sizes, const uint32_t sets_count) {
    Destroy();

    descr_counts_[int(eDescrType::CombinedImageSampler)] = sizes.img_sampler_count;
    descr_counts_[int(eDescrType::StorageImage)] = sizes.store_img_count;
    descr_counts_[int(eDescrType::UniformBuffer)] = sizes.ubuf_count;
    descr_counts_[int(eDescrType::UniformTexBuffer)] = sizes.utbuf_count;
    descr_counts_[int(eDescrType::StorageBuffer)] = sizes.sbuf_count;
    descr_counts_[int(eDescrType::StorageTexBuffer)] = sizes.stbuf_count;
    if (api_ctx_->raytracing_supported) {
        descr_counts_[int(eDescrType::AccStructure)] = sizes.acc_count;
    }

    SmallVector<VkDescriptorPoolSize, int(eDescrType::_Count)> pool_sizes;
    for (int i = 0; i < int(eDescrType::_Count); ++i) {
        if (descr_counts_[i]) {
            pool_sizes.push_back({g_descr_types_vk[i], sets_count * descr_counts_[i]});
        }
    }

    VkDescriptorPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.cdata();
    pool_info.maxSets = sets_count;

    sets_count_ = sets_count;

    const VkResult res = api_ctx_->vkCreateDescriptorPool(api_ctx_->device, &pool_info, nullptr, &handle_);
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

    VkDescriptorSetAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = handle_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet descr_set = VK_NULL_HANDLE;
    const VkResult res = api_ctx_->vkAllocateDescriptorSets(api_ctx_->device, &alloc_info, &descr_set);
    assert(res == VK_SUCCESS);

    ++next_free_;

    return descr_set;
}

bool Ren::DescrPool::Reset() {
    next_free_ = 0;
    const VkResult res = api_ctx_->vkResetDescriptorPool(api_ctx_->device, handle_, 0);
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
            if (!new_pool.Init(sizes_, count_mul * initial_sets_count_)) {
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
                                              const uint32_t max_img_sampler_count, const uint32_t max_img_count,
                                              const uint32_t max_sampler_count, const uint32_t max_store_img_count,
                                              const uint32_t max_ubuf_count, const uint32_t max_tbuf_count,
                                              const uint32_t max_sbuf_count, const uint32_t max_stbuf_count,
                                              const uint32_t max_acc_count, const uint32_t initial_sets_count)
    : pool_step_(pool_step) {
    img_sampler_based_count_ = (max_img_sampler_count + pool_step - 1) / pool_step;
    img_based_count_ = (max_img_count + pool_step - 1) / pool_step;
    sampler_based_count_ = (max_sampler_count + pool_step - 1) / pool_step;
    store_img_based_count_ = (max_store_img_count + pool_step - 1) / pool_step;
    ubuf_based_count_ = (max_ubuf_count + pool_step - 1) / pool_step;
    utbuf_based_count_ = (max_tbuf_count + pool_step - 1) / pool_step;
    sbuf_based_count_ = (max_sbuf_count + pool_step - 1) / pool_step;
    stbuf_based_count_ = (max_stbuf_count + pool_step - 1) / pool_step;
    acc_based_count_ = (max_acc_count + pool_step - 1) / pool_step;
    const uint32_t required_pools_count = img_sampler_based_count_ * img_based_count_ * sampler_based_count_ *
                                          store_img_based_count_ * ubuf_based_count_ * utbuf_based_count_ *
                                          sbuf_based_count_ * stbuf_based_count_ * std::max(acc_based_count_, 1u);

    // store rounded values
    max_img_sampler_count_ = pool_step * img_sampler_based_count_;
    max_img_count_ = pool_step * img_based_count_;
    max_sampler_count_ = pool_step * sampler_based_count_;
    max_store_img_count_ = pool_step * store_img_based_count_;
    max_ubuf_count_ = pool_step * ubuf_based_count_;
    max_utbuf_count_ = pool_step * utbuf_based_count_;
    max_sbuf_count_ = pool_step * sbuf_based_count_;
    max_stbuf_count_ = pool_step * stbuf_based_count_;
    max_acc_count_ = pool_step * acc_based_count_;

    for (uint32_t i = 0; i < required_pools_count; ++i) {
        uint32_t index = i;

        DescrSizes pool_sizes = {};
        pool_sizes.acc_count = pool_step * ((index % acc_based_count_) + 1);
        index /= acc_based_count_;
        pool_sizes.stbuf_count = pool_step * ((index % stbuf_based_count_) + 1);
        index /= stbuf_based_count_;
        pool_sizes.sbuf_count = pool_step * ((index % sbuf_based_count_) + 1);
        index /= sbuf_based_count_;
        pool_sizes.utbuf_count = pool_step * ((index % utbuf_based_count_) + 1);
        index /= utbuf_based_count_;
        pool_sizes.ubuf_count = pool_step * ((index % ubuf_based_count_) + 1);
        index /= ubuf_based_count_;
        pool_sizes.store_img_count = pool_step * ((index % store_img_based_count_) + 1);
        index /= store_img_based_count_;
        pool_sizes.sampler_count = pool_step * ((index % sampler_based_count_) + 1);
        index /= sampler_based_count_;
        pool_sizes.img_count = pool_step * ((index % img_based_count_) + 1);
        index /= img_based_count_;
        pool_sizes.img_sampler_count = pool_step * ((index % img_sampler_based_count_) + 1);
        index /= img_sampler_based_count_;

        pools_.emplace_back(api_ctx, pool_sizes, initial_sets_count);
    }
    assert(pools_.size() == required_pools_count);
}

VkDescriptorSet Ren::DescrMultiPoolAlloc::Alloc(const DescrSizes &sizes, const VkDescriptorSetLayout layout) {
    assert(sizes.acc_count <= max_acc_count_);
    assert(sizes.stbuf_count <= max_stbuf_count_);
    assert(sizes.sbuf_count <= max_sbuf_count_);
    assert(sizes.utbuf_count <= max_utbuf_count_);
    assert(sizes.ubuf_count <= max_ubuf_count_);
    assert(sizes.store_img_count <= max_store_img_count_);
    assert(sizes.sampler_count <= max_sampler_count_);
    assert(sizes.img_count <= max_img_count_);
    assert(sizes.img_sampler_count <= max_img_sampler_count_);

    const uint32_t img_sampler_based_index =
        sizes.img_sampler_count ? ((sizes.img_sampler_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t img_based_index = sizes.img_count ? ((sizes.img_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t sampler_based_index =
        sizes.sampler_count ? ((sizes.sampler_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t store_img_based_index =
        sizes.store_img_count ? ((sizes.store_img_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t ubuf_based_index = sizes.ubuf_count ? ((sizes.ubuf_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t utbuf_based_index = sizes.utbuf_count ? ((sizes.utbuf_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t sbuf_based_index = sizes.sbuf_count ? ((sizes.sbuf_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t stbuf_based_index = sizes.stbuf_count ? ((sizes.stbuf_count + pool_step_ - 1) / pool_step_ - 1) : 0;
    const uint32_t acc_based_index = sizes.acc_count ? ((sizes.acc_count + pool_step_ - 1) / pool_step_ - 1) : 0;

    uint32_t pool_index = img_sampler_based_index * img_based_count_ * sampler_based_count_ * store_img_based_count_ *
                          ubuf_based_count_ * utbuf_based_count_ * sbuf_based_count_ * stbuf_based_count_ *
                          acc_based_count_;
    pool_index += img_based_index * sampler_based_count_ * store_img_based_count_ * ubuf_based_count_ *
                  utbuf_based_count_ * sbuf_based_count_ * stbuf_based_count_ * acc_based_count_;
    pool_index += sampler_based_index * store_img_based_count_ * ubuf_based_count_ * utbuf_based_count_ *
                  sbuf_based_count_ * stbuf_based_count_ * acc_based_count_;
    pool_index += store_img_based_index * ubuf_based_count_ * utbuf_based_count_ * sbuf_based_count_ *
                  stbuf_based_count_ * acc_based_count_;
    pool_index += ubuf_based_index * utbuf_based_count_ * sbuf_based_count_ * stbuf_based_count_ * acc_based_count_;
    pool_index += utbuf_based_index * sbuf_based_count_ * stbuf_based_count_ * acc_based_count_;
    pool_index += sbuf_based_index * stbuf_based_count_ * acc_based_count_;
    pool_index += stbuf_based_index * acc_based_count_;
    pool_index += acc_based_index;

    return pools_[pool_index].Alloc(layout);
}

bool Ren::DescrMultiPoolAlloc::Reset() {
    bool result = true;
    for (auto &pool : pools_) {
        result &= pool.Reset();
    }
    return result;
}
