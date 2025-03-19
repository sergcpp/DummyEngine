#include "SamplerVK.h"

#include "VKCtx.h"

namespace Ren {
#define X(_0, _1, _2, _3, _4) _1,
extern const VkFilter g_min_mag_filter_vk[] = {
#include "TextureFilter.inl"
};
#undef X

#define X(_0, _1, _2, _3, _4) _2,
extern const VkSamplerMipmapMode g_mipmap_mode_vk[] = {
#include "TextureFilter.inl"
};
#undef X

#define X(_0, _1, _2) _1,
extern const VkSamplerAddressMode g_wrap_mode_vk[] = {
#include "../TextureWrap.inl"
};
#undef X

#define X(_0, _1, _2) _1,
extern const VkCompareOp g_compare_ops_vk[] = {
#include "../TextureCompare.inl"
};
#undef X

extern const float AnisotropyLevel = 4;
} // namespace Ren

Ren::Sampler &Ren::Sampler::operator=(Sampler &&rhs) noexcept {
    if (&rhs == this) {
        return (*this);
    }

    Destroy();

    RefCounter::operator=(std::move(rhs));

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    params_ = std::exchange(rhs.params_, {});

    return (*this);
}

void Ren::Sampler::Destroy() {
    if (handle_) {
        api_ctx_->samplers_to_destroy[api_ctx_->backend_frame].emplace_back(handle_);
        handle_ = {};
    }
}

void Ren::Sampler::Init(ApiContext *api_ctx, const SamplingParams params) {
    Destroy();

    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = g_min_mag_filter_vk[size_t(params.filter)];
    sampler_info.minFilter = g_min_mag_filter_vk[size_t(params.filter)];
    sampler_info.addressModeU = g_wrap_mode_vk[size_t(params.wrap)];
    sampler_info.addressModeV = g_wrap_mode_vk[size_t(params.wrap)];
    sampler_info.addressModeW = g_wrap_mode_vk[size_t(params.wrap)];
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = AnisotropyLevel;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = (params.compare != eTexCompare::None) ? VK_TRUE : VK_FALSE;
    sampler_info.compareOp = g_compare_ops_vk[size_t(params.compare)];
    sampler_info.mipmapMode = g_mipmap_mode_vk[size_t(params.filter)];
    sampler_info.mipLodBias = params.lod_bias.to_float();
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;

    const VkResult res = api_ctx->vkCreateSampler(api_ctx->device, &sampler_info, nullptr, &handle_);
    assert(res == VK_SUCCESS && "Failed to create sampler!");

    api_ctx_ = api_ctx;
    params_ = params;
}
