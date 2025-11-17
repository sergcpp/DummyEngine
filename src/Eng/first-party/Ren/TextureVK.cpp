#include "TextureVK.h"

#include <memory>

#include "Config.h"
#include "Log.h"
#include "TextureParams.h"
#include "Utils.h"
#include "VKCtx.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#ifndef NDEBUG
// #define TEX_VERBOSE_LOGGING
#endif

namespace Ren {
#define X(_0, _1, _2, _3, _4, _5, _6, _7, _8) _5,
extern const VkFormat g_formats_vk[] = {
#include "TextureFormat.inl"
};
#undef X

uint32_t TextureHandleCounter = 0;

// make sure we can simply cast these
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT == 1);
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT == 2);
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT == 4);
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT == 8);

VkImageUsageFlags to_vk_image_usage(const Bitmask<eTexUsage> usage, const eTexFormat format) {
    VkImageUsageFlags ret = 0;
    if (usage & eTexUsage::Transfer) {
        ret |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }
    if (usage & eTexUsage::Sampled) {
        ret |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (usage & eTexUsage::Storage) {
        assert(!IsCompressedFormat(format));
        ret |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (usage & eTexUsage::RenderTarget) {
        assert(!IsCompressedFormat(format));
        if (IsDepthFormat(format)) {
            ret |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            ret |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }
    return ret;
}

eTexFormat FormatFromGLInternalFormat(uint32_t gl_internal_format, bool *is_srgb);
int GetBlockLenBytes(eTexFormat format);
int GetDataLenBytes(int w, int h, int d, eTexFormat format);
void ParseDDSHeader(const DDSHeader &hdr, TexParams *params);

extern const VkFilter g_min_mag_filter_vk[];
extern const VkSamplerAddressMode g_wrap_mode_vk[];
extern const VkSamplerMipmapMode g_mipmap_mode_vk[];
extern const VkCompareOp g_compare_ops_vk[];

extern const float AnisotropyLevel;
} // namespace Ren

Ren::Texture::Texture(std::string_view name, ApiContext *api_ctx, const TexParams &p, MemAllocators *mem_allocs,
                      ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(p, mem_allocs, log);
}

Ren::Texture::Texture(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data, const TexParams &p,
                      MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(data, p, mem_allocs, load_status, log);
}

Ren::Texture::Texture(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data[6], const TexParams &p,
                      MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(data, p, mem_allocs, load_status, log);
}

Ren::Texture::~Texture() { Free(); }

Ren::Texture &Ren::Texture::operator=(Texture &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    alloc_ = std::exchange(rhs.alloc_, {});
    params = std::exchange(rhs.params, {});
    name_ = std::move(rhs.name_);

    resource_state = std::exchange(rhs.resource_state, eResState::Undefined);

    return (*this);
}

void Ren::Texture::Init(const TexParams &p, MemAllocators *mem_allocs, ILog *log) {
    InitFromRAWData(nullptr, 0, nullptr, mem_allocs, p, log);
}

void Ren::Texture::Init(const TexHandle &handle, const TexParams &_params, MemAllocation &&alloc, ILog *log) {
    handle_ = handle;
    alloc_ = std::move(alloc);
    params = _params;

    if (handle.views[0] == VkImageView{} && _params.usage != Bitmask(eTexUsage::Transfer) &&
        !(Bitmask<eTexFlags>{params.flags} & eTexFlags::NoOwnership)) {
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = handle_.img;
        view_info.viewType = (_params.flags & eTexFlags::Array)
                                 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                 : (params.d ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D);
        view_info.format = g_formats_vk[size_t(params.format)];
        if (IsDepthStencilFormat(params.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        } else if (IsDepthFormat(params.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        } else {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views[0]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }

        if (IsDepthStencilFormat(params.format)) {
            // create additional depth-only image view
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            VkImageView depth_only_view;
            const VkResult _res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &depth_only_view);
            if (_res != VK_SUCCESS) {
                log->Error("Failed to create image view!");
                return;
            }
            handle_.views.push_back(depth_only_view);
        }

#ifdef ENABLE_GPU_DEBUG
        for (VkImageView view : handle_.views) {
            VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
            name_info.objectHandle = uint64_t(view);
            name_info.pObjectName = name_.c_str();
            api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
        }
#endif
    }

    if (handle_.sampler == VkSampler{} && !(Bitmask<eTexFlags>{params.flags} & eTexFlags::NoOwnership)) {
        SetSampling(params.sampling);
    }
}

void Ren::Texture::Init(Span<const uint8_t> data, const TexParams &p, MemAllocators *mem_allocs,
                        eTexLoadStatus *load_status, ILog *log) {
    assert(!data.empty());

    auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload, uint32_t(data.size())};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        memcpy(stage_data, data.data(), data.size());
        sbuf.Unmap();
    }
    CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();
    InitFromRAWData(&sbuf, 0, cmd_buf, mem_allocs, p, log);
    api_ctx_->EndSingleTimeCommands(cmd_buf);
    sbuf.FreeImmediate();

    (*load_status) = eTexLoadStatus::CreatedFromData;
}

void Ren::Texture::Init(Span<const uint8_t> data[6], const TexParams &p, MemAllocators *mem_allocs,
                        eTexLoadStatus *load_status, ILog *log) {
    assert(data);

    auto sbuf = Buffer{
        "Temp Stage Buf", api_ctx_, eBufType::Upload,
        uint32_t(data[0].size() + data[1].size() + data[2].size() + data[3].size() + data[4].size() + data[5].size())};
    int data_off[6];
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        uint32_t stage_off = 0;

        for (int i = 0; i < 6; i++) {
            if (!data[i].empty()) {
                memcpy(&stage_data[stage_off], data[i].data(), data[i].size());
                data_off[i] = int(stage_off);
                stage_off += uint32_t(data[i].size());
            } else {
                data_off[i] = -1;
            }
        }
        sbuf.Unmap();
    }
    CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();
    InitFromRAWData(sbuf, data_off, cmd_buf, mem_allocs, p, log);
    api_ctx_->EndSingleTimeCommands(cmd_buf);
    sbuf.FreeImmediate();

    (*load_status) = eTexLoadStatus::CreatedFromData;
}

void Ren::Texture::Free() {
    if (params.format != eTexFormat::Undefined && !(Bitmask<eTexFlags>{params.flags} & eTexFlags::NoOwnership)) {
        for (VkImageView view : handle_.views) {
            if (view) {
                api_ctx_->image_views_to_destroy[api_ctx_->backend_frame].push_back(view);
            }
        }
        api_ctx_->images_to_destroy[api_ctx_->backend_frame].push_back(handle_.img);
        api_ctx_->samplers_to_destroy[api_ctx_->backend_frame].push_back(handle_.sampler);
        api_ctx_->allocations_to_free[api_ctx_->backend_frame].emplace_back(std::move(alloc_));

        handle_ = {};
        params.format = eTexFormat::Undefined;
    }
}

void Ren::Texture::FreeImmediate() {
    if (params.format != eTexFormat::Undefined && !(Bitmask<eTexFlags>{params.flags} & eTexFlags::NoOwnership)) {
        for (VkImageView view : handle_.views) {
            if (view) {
                api_ctx_->vkDestroyImageView(api_ctx_->device, view, nullptr);
            }
        }
        api_ctx_->vkDestroyImage(api_ctx_->device, handle_.img, nullptr);
        api_ctx_->vkDestroySampler(api_ctx_->device, handle_.sampler, nullptr);
        alloc_.Release();

        handle_ = {};
        params.format = eTexFormat::Undefined;
    }
}

bool Ren::Texture::Realloc(const int w, const int h, int mip_count, const int samples, const eTexFormat format,
                           CommandBuffer cmd_buf, MemAllocators *mem_allocs, ILog *log) {
    VkImage new_image = VK_NULL_HANDLE;
    VkImageView new_image_view = VK_NULL_HANDLE;
    MemAllocation new_alloc = {};
    eResState new_resource_state = eResState::Undefined;

    if (!mip_count) {
        mip_count = CalcMipCount(w, h, 1);
    }

    { // create new image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(w);
        img_info.extent.height = uint32_t(h);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count;
        img_info.arrayLayers = 1;
        img_info.format = g_formats_vk[size_t(format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        assert(uint8_t(params.usage) != 0);
        img_info.usage = to_vk_image_usage(Bitmask<eTexUsage>{params.usage}, format);

        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VkSampleCountFlagBits(samples);
        img_info.flags = 0;

        VkResult res = api_ctx_->vkCreateImage(api_ctx_->device, &img_info, nullptr, &new_image);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return false;
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(new_image);
        name_info.pObjectName = name_.c_str();
        api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        api_ctx_->vkGetImageMemoryRequirements(api_ctx_->device, new_image, &tex_mem_req);

        VkMemoryPropertyFlags img_tex_desired_mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        new_alloc = mem_allocs->Allocate(tex_mem_req, img_tex_desired_mem_flags);
        if (!new_alloc) {
            log->Warning("Not enough device memory, falling back to CPU RAM!");
            img_tex_desired_mem_flags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            new_alloc = mem_allocs->Allocate(tex_mem_req, img_tex_desired_mem_flags);
        }

        res = api_ctx_->vkBindImageMemory(api_ctx_->device, new_image, new_alloc.owner->mem(new_alloc.pool),
                                          new_alloc.offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return false;
        }
    }

    { // create new image view
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = new_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = g_formats_vk[size_t(format)];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &new_image_view);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return false;
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
        name_info.objectHandle = uint64_t(new_image_view);
        name_info.pObjectName = name_.c_str();
        api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif
    }

#ifdef TEX_VERBOSE_LOGGING
    if (params_.format != eTexFormat::Undefined) {
        log->Info("Realloc %s, %ix%i (%i mips) -> %ix%i (%i mips)", name_.c_str(), int(params_.w), int(params_.h),
                  int(params_.mip_count), w, h, mip_count);
    } else {
        log->Info("Alloc %s %ix%i (%i mips)", name_.c_str(), w, h, mip_count);
    }
#endif

    const TexHandle new_handle = {new_image, new_image_view, VK_NULL_HANDLE, std::exchange(handle_.sampler, {}),
                                  TextureHandleCounter++};

    // copy data from old texture
    if (params.format == format) {
        int src_mip = 0, dst_mip = 0;
        while (std::max(params.w >> src_mip, 1) != std::max(w >> dst_mip, 1) ||
               std::max(params.h >> src_mip, 1) != std::max(h >> dst_mip, 1)) {
            if (std::max(params.w >> src_mip, 1) > std::max(w >> dst_mip, 1) ||
                std::max(params.h >> src_mip, 1) > std::max(h >> dst_mip, 1)) {
                ++src_mip;
            } else {
                ++dst_mip;
            }
        }

        VkImageCopy copy_regions[16];
        uint32_t copy_regions_count = 0;

        for (; src_mip < int(params.mip_count) && dst_mip < mip_count; ++src_mip, ++dst_mip) {
            VkImageCopy &reg = copy_regions[copy_regions_count++];

            reg.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            reg.srcSubresource.baseArrayLayer = 0;
            reg.srcSubresource.layerCount = 1;
            reg.srcSubresource.mipLevel = src_mip;
            reg.srcOffset = {0, 0, 0};
            reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            reg.dstSubresource.baseArrayLayer = 0;
            reg.dstSubresource.layerCount = 1;
            reg.dstSubresource.mipLevel = dst_mip;
            reg.dstOffset = {0, 0, 0};
            reg.extent = {uint32_t(std::max(w >> dst_mip, 1)), uint32_t(std::max(h >> dst_mip, 1)), 1};

#ifdef TEX_VERBOSE_LOGGING
            log->Info("Copying data mip %i [old] -> mip %i [new]", src_mip, dst_mip);
#endif
        }

        if (copy_regions_count) {
            VkPipelineStageFlags src_stages = 0, dst_stages = 0;
            SmallVector<VkImageMemoryBarrier, 2> barriers;

            // src image barrier
            if (this->resource_state != eResState::CopySrc) {
                auto &new_barrier = barriers.emplace_back();
                new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
                new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
                new_barrier.oldLayout = VkImageLayout(VKImageLayoutForState(this->resource_state));
                new_barrier.newLayout = VkImageLayout(VKImageLayoutForState(eResState::CopySrc));
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.image = handle_.img;
                new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                new_barrier.subresourceRange.baseMipLevel = 0;
                new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                new_barrier.subresourceRange.baseArrayLayer = 0;
                new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                src_stages |= VKPipelineStagesForState(this->resource_state);
                dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
            }

            // dst image barrier
            if (new_resource_state != eResState::CopyDst) {
                auto &new_barrier = barriers.emplace_back();
                new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                new_barrier.srcAccessMask = VKAccessFlagsForState(new_resource_state);
                new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
                new_barrier.oldLayout = VkImageLayout(VKImageLayoutForState(new_resource_state));
                new_barrier.newLayout = VkImageLayout(VKImageLayoutForState(eResState::CopyDst));
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.image = new_image;
                new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                new_barrier.subresourceRange.baseMipLevel = 0;
                new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                new_barrier.subresourceRange.baseArrayLayer = 0;
                new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                src_stages |= VKPipelineStagesForState(new_resource_state);
                dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
            }

            src_stages &= api_ctx_->supported_stages_mask;
            dst_stages &= api_ctx_->supported_stages_mask;

            if (!barriers.empty()) {
                api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                               dst_stages, 0, 0, nullptr, 0, nullptr, uint32_t(barriers.size()),
                                               barriers.cdata());
            }

            this->resource_state = eResState::CopySrc;
            new_resource_state = eResState::CopyDst;

            api_ctx_->vkCmdCopyImage(cmd_buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, new_image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_regions_count, copy_regions);
        }
    }
    Free();

    handle_ = new_handle;
    alloc_ = std::move(new_alloc);
    params.w = w;
    params.h = h;
    params.mip_count = mip_count;
    params.samples = samples;
    params.format = format;

    resource_state = new_resource_state;

    return true;
}

void Ren::Texture::InitFromRAWData(Buffer *sbuf, int data_off, CommandBuffer cmd_buf, MemAllocators *mem_allocs,
                                   const TexParams &p, ILog *log) {
    Free();

    handle_.generation = TextureHandleCounter++;
    params = p;

    int mip_count = params.mip_count;
    if (!mip_count) {
        mip_count = CalcMipCount(p.w, p.h, 1);
    }

    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType =
            (p.flags & eTexFlags::Array) ? VK_IMAGE_TYPE_2D : (p.d ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D);
        img_info.extent.width = p.w;
        img_info.extent.height = p.h;
        img_info.extent.depth = (p.flags & eTexFlags::Array) ? 1 : std::max<uint32_t>(p.d, 1u);
        img_info.mipLevels = mip_count;
        img_info.arrayLayers = (p.flags & eTexFlags::Array) ? std::max<uint32_t>(p.d, 1u) : 1;
        img_info.format = g_formats_vk[size_t(p.format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        assert(uint8_t(p.usage) != 0);
        img_info.usage = to_vk_image_usage(p.usage, p.format);

        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VkSampleCountFlagBits(p.samples);
        img_info.flags = 0;

        VkResult res = api_ctx_->vkCreateImage(api_ctx_->device, &img_info, nullptr, &handle_.img);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return;
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(handle_.img);
        name_info.pObjectName = name_.c_str();
        api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        api_ctx_->vkGetImageMemoryRequirements(api_ctx_->device, handle_.img, &tex_mem_req);

        VkMemoryPropertyFlags img_tex_desired_mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        alloc_ = mem_allocs->Allocate(tex_mem_req, img_tex_desired_mem_flags);
        if (!alloc_) {
            log->Warning("Not enough device memory, falling back to CPU RAM!");
            img_tex_desired_mem_flags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            alloc_ = mem_allocs->Allocate(tex_mem_req, img_tex_desired_mem_flags);
        }

        if (!alloc_) {
            log->Error("Failed to allocate memory!");
            return;
        }

        res = api_ctx_->vkBindImageMemory(api_ctx_->device, handle_.img, alloc_.owner->mem(alloc_.pool), alloc_.offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return;
        }
    }

    if (p.usage != Bitmask(eTexUsage::Transfer)) { // not 'transfer only'
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = handle_.img;
        view_info.viewType = (p.flags & eTexFlags::Array) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                                          : (p.d ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D);
        view_info.format = g_formats_vk[size_t(p.format)];
        if (IsDepthStencilFormat(p.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        } else if (IsDepthFormat(p.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        } else {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views[0]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }

        if (IsDepthStencilFormat(p.format)) {
            // create additional depth-only image view
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            VkImageView depth_only_view;
            const VkResult _res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &depth_only_view);
            if (_res != VK_SUCCESS) {
                log->Error("Failed to create image view!");
                return;
            }
            handle_.views.push_back(depth_only_view);
        }

#ifdef ENABLE_GPU_DEBUG
        for (VkImageView view : handle_.views) {
            VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
            name_info.objectHandle = uint64_t(view);
            name_info.pObjectName = name_.c_str();
            api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
        }
#endif
    }

    this->resource_state = eResState::Undefined;

    if (sbuf) {
        assert(p.samples == 1);
        assert(sbuf && sbuf->type() == eBufType::Upload);

        VkPipelineStageFlags src_stages = 0, dst_stages = 0;
        SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;
        SmallVector<VkImageMemoryBarrier, 1> img_barriers;

        if (sbuf->resource_state != eResState::Undefined && sbuf->resource_state != eResState::CopySrc) {
            auto &new_barrier = buf_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            new_barrier.srcAccessMask = VKAccessFlagsForState(sbuf->resource_state);
            new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.buffer = sbuf->vk_handle();
            new_barrier.offset = VkDeviceSize(data_off);
            new_barrier.size = VkDeviceSize(sbuf->size() - data_off);

            src_stages |= VKPipelineStagesForState(sbuf->resource_state);
            dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
        }

        if (this->resource_state != eResState::CopyDst) {
            auto &new_barrier = img_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
            new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
            new_barrier.oldLayout = VkImageLayout(VKImageLayoutForState(this->resource_state));
            new_barrier.newLayout = VkImageLayout(VKImageLayoutForState(eResState::CopyDst));
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.image = handle_.img;
            new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            new_barrier.subresourceRange.baseMipLevel = 0;
            new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            new_barrier.subresourceRange.baseArrayLayer = 0;
            new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            src_stages |= VKPipelineStagesForState(this->resource_state);
            dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
        }

        src_stages &= api_ctx_->supported_stages_mask;
        dst_stages &= api_ctx_->supported_stages_mask;

        if (!buf_barriers.empty() || !img_barriers.empty()) {
            api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           dst_stages, 0, 0, nullptr, uint32_t(buf_barriers.size()),
                                           buf_barriers.cdata(), uint32_t(img_barriers.size()), img_barriers.cdata());
        }

        sbuf->resource_state = eResState::CopySrc;
        this->resource_state = eResState::CopyDst;

        int w = p.w, h = p.h, d = p.d;
        int bytes_left = sbuf->size() - data_off;
        for (int i = 0; i < mip_count; ++i) {
            const int len = GetDataLenBytes(w, h, d, p.format);
            if (len > bytes_left) {
                log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
                return;
            }

            VkBufferImageCopy region = {};
            region.bufferOffset = VkDeviceSize(data_off);
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = i;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = (p.flags & eTexFlags::Array) ? d : 1;

            region.imageOffset = {0, 0, 0};
            region.imageExtent = {uint32_t(w), uint32_t(h),
                                  uint32_t((p.flags & eTexFlags::Array) ? 1 : std::max(d, 1))};

            api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf->vk_handle(), handle_.img,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            data_off += len;
            bytes_left -= len;
            w = std::max(w / 2, 1);
            h = std::max(h / 2, 1);
            d = std::max(d / 2, 1);
        }
    }

    { // create new sampler
        VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler_info.magFilter = g_min_mag_filter_vk[size_t(p.sampling.filter)];
        sampler_info.minFilter = g_min_mag_filter_vk[size_t(p.sampling.filter)];
        sampler_info.addressModeU = g_wrap_mode_vk[size_t(p.sampling.wrap)];
        sampler_info.addressModeV = g_wrap_mode_vk[size_t(p.sampling.wrap)];
        sampler_info.addressModeW = g_wrap_mode_vk[size_t(p.sampling.wrap)];
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = AnisotropyLevel;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = p.sampling.compare != eTexCompare::None ? VK_TRUE : VK_FALSE;
        sampler_info.compareOp = g_compare_ops_vk[size_t(p.sampling.compare)];
        sampler_info.mipmapMode = g_mipmap_mode_vk[size_t(p.sampling.filter)];
        sampler_info.mipLodBias = p.sampling.lod_bias.to_float();
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = VK_LOD_CLAMP_NONE;

        const VkResult res = api_ctx_->vkCreateSampler(api_ctx_->device, &sampler_info, nullptr, &handle_.sampler);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create sampler!");
        }
    }
}

void Ren::Texture::InitFromRAWData(Buffer &sbuf, int data_off[6], CommandBuffer cmd_buf, MemAllocators *mem_allocs,
                                   const TexParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    handle_.generation = TextureHandleCounter++;
    params = p;

    const int mip_count = CalcMipCount(p.w, p.h, 1);
    params.mip_count = mip_count;

    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count;
        img_info.arrayLayers = 6;
        img_info.format = g_formats_vk[size_t(p.format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        assert(uint8_t(p.usage) != 0);
        img_info.usage = to_vk_image_usage(p.usage, p.format);
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VkSampleCountFlagBits(p.samples);
        img_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VkResult res = api_ctx_->vkCreateImage(api_ctx_->device, &img_info, nullptr, &handle_.img);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return;
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(handle_.img);
        name_info.pObjectName = name_.c_str();
        api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        api_ctx_->vkGetImageMemoryRequirements(api_ctx_->device, handle_.img, &tex_mem_req);

        VkMemoryPropertyFlags img_tex_desired_mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        alloc_ = mem_allocs->Allocate(tex_mem_req, img_tex_desired_mem_flags);
        if (!alloc_) {
            log->Warning("Not enough device memory, falling back to CPU RAM!");
            img_tex_desired_mem_flags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            alloc_ = mem_allocs->Allocate(tex_mem_req, img_tex_desired_mem_flags);
        }

        res = api_ctx_->vkBindImageMemory(api_ctx_->device, handle_.img, alloc_.owner->mem(alloc_.pool), alloc_.offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return;
        }
    }

    { // create default image view
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = handle_.img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view_info.format = g_formats_vk[size_t(p.format)];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views[0]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
        name_info.objectHandle = uint64_t(handle_.views[0]);
        name_info.pObjectName = name_.c_str();
        api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif
    }

    assert(p.samples == 1);
    assert(sbuf.type() == eBufType::Upload);

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;
    SmallVector<VkImageMemoryBarrier, 1> img_barriers;

    if (sbuf.resource_state != eResState::Undefined && sbuf.resource_state != eResState::CopySrc) {
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(sbuf.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = sbuf.vk_handle();
        new_barrier.offset = VkDeviceSize(0);
        new_barrier.size = VkDeviceSize(sbuf.size());

        src_stages |= VKPipelineStagesForState(sbuf.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    if (this->resource_state != eResState::CopyDst) {
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.oldLayout = VkImageLayout(VKImageLayoutForState(this->resource_state));
        new_barrier.newLayout = VkImageLayout(VKImageLayoutForState(eResState::CopyDst));
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = handle_.img;
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, buf_barriers.size(), buf_barriers.cdata(), img_barriers.size(),
                                       img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    SmallVector<VkBufferImageCopy, 16> regions;
    for (int i = 0; i < 6; i++) {
        VkDeviceSize buffer_offset = data_off[i];
        for (int j = 0; j < mip_count; ++j) {
            VkBufferImageCopy &reg = regions.emplace_back();

            reg.bufferOffset = buffer_offset;
            reg.bufferRowLength = 0;
            reg.bufferImageHeight = 0;

            reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            reg.imageSubresource.mipLevel = j;
            reg.imageSubresource.baseArrayLayer = i;
            reg.imageSubresource.layerCount = 1;

            reg.imageOffset = {0, 0, 0};
            reg.imageExtent = {uint32_t(p.w) >> j, uint32_t(p.h) >> j, 1};

            buffer_offset += GetDataLenBytes(int(reg.imageExtent.width), int(reg.imageExtent.height), 1, p.format);
        }
    }

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     uint32_t(regions.size()), regions.data());

    ApplySampling(p.sampling, log);
}

int Ren::Texture::AddImageView(const eTexFormat format, const int mip_level, const int mip_count, const int base_layer,
                               const int layer_count) {
    const TexParams p = params;

    VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image = handle_.img;
    view_info.viewType = (p.flags & eTexFlags::Array) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                                      : (p.d ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D);
    view_info.format = g_formats_vk[size_t(format)];
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = mip_level;
    view_info.subresourceRange.levelCount = mip_count;
    view_info.subresourceRange.baseArrayLayer = base_layer;
    view_info.subresourceRange.layerCount = layer_count;

    handle_.views.emplace_back(VK_NULL_HANDLE);
    const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views.back());
    if (res != VK_SUCCESS) {
        return -1;
    }

#ifdef ENABLE_GPU_DEBUG
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    name_info.objectHandle = uint64_t(handle_.views.back());
    name_info.pObjectName = name_.c_str();
    api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

    return int(handle_.views.size()) - 1;
}

void Ren::Texture::SetSubImage(const int layer, const int level, const int offsetx, const int offsety,
                               const int offsetz, const int sizex, const int sizey, const int sizez,
                               const eTexFormat format, const Buffer &sbuf, CommandBuffer cmd_buf, const int data_off,
                               const int data_len) {
    assert(format == params.format);
    assert(params.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params.h >> level, 1));
    assert(offsetz >= 0 && offsetz + sizez <= std::max(params.d >> level, 1));
    assert(sbuf.type() == eBufType::Upload);

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;
    SmallVector<VkImageMemoryBarrier, 1> img_barriers;

    if (sbuf.resource_state != eResState::Undefined && sbuf.resource_state != eResState::CopySrc) {
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(sbuf.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = sbuf.vk_handle();
        new_barrier.offset = VkDeviceSize(0);
        new_barrier.size = VkDeviceSize(sbuf.size());

        src_stages |= VKPipelineStagesForState(sbuf.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    if (this->resource_state != eResState::CopyDst) {
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.oldLayout = VkImageLayout(VKImageLayoutForState(this->resource_state));
        new_barrier.newLayout = VkImageLayout(VKImageLayoutForState(eResState::CopyDst));
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = handle_.img;
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, buf_barriers.size(), buf_barriers.cdata(), img_barriers.size(),
                                       img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy region = {};

    region.bufferOffset = VkDeviceSize(data_off);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = uint32_t(level);
    region.imageSubresource.baseArrayLayer = uint32_t(layer);
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {int32_t(offsetx), int32_t(offsety), int32_t(offsetz)};
    region.imageExtent = {uint32_t(sizex), uint32_t(sizey), uint32_t(sizez)};

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                     &region);
}

void Ren::Texture::CopyTextureData(const Buffer &sbuf, CommandBuffer cmd_buf, const int data_off,
                                   const int data_len) const {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;
    SmallVector<VkImageMemoryBarrier, 1> img_barriers;

    if (this->resource_state != eResState::CopySrc) {
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.oldLayout = VkImageLayout(VKImageLayoutForState(this->resource_state));
        new_barrier.newLayout = VkImageLayout(VKImageLayoutForState(eResState::CopySrc));
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = handle_.img;
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    if (sbuf.resource_state != eResState::Undefined && sbuf.resource_state != eResState::CopyDst) {
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(sbuf.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = sbuf.vk_handle();
        new_barrier.offset = VkDeviceSize(0);
        new_barrier.size = VkDeviceSize(sbuf.size());

        src_stages |= VKPipelineStagesForState(sbuf.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, buf_barriers.size(), buf_barriers.cdata(), img_barriers.size(),
                                       img_barriers.cdata());
    }

    this->resource_state = eResState::CopySrc;
    sbuf.resource_state = eResState::CopyDst;

    VkBufferImageCopy region = {};

    region.bufferOffset = VkDeviceSize(data_off);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = uint32_t(0);
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {int32_t(0), int32_t(0), 0};
    region.imageExtent = {uint32_t(params.w), uint32_t(params.h), 1};

    api_ctx_->vkCmdCopyImageToBuffer(cmd_buf, handle_.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sbuf.vk_handle(), 1,
                                     &region);
}

void Ren::Texture::SetSampling(const SamplingParams s) {
    if (handle_.sampler) {
        api_ctx_->samplers_to_destroy[api_ctx_->backend_frame].emplace_back(handle_.sampler);
    }

    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = g_min_mag_filter_vk[size_t(s.filter)];
    sampler_info.minFilter = g_min_mag_filter_vk[size_t(s.filter)];
    sampler_info.addressModeU = g_wrap_mode_vk[size_t(s.wrap)];
    sampler_info.addressModeV = g_wrap_mode_vk[size_t(s.wrap)];
    sampler_info.addressModeW = g_wrap_mode_vk[size_t(s.wrap)];
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = AnisotropyLevel;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = s.compare != eTexCompare::None ? VK_TRUE : VK_FALSE;
    sampler_info.compareOp = g_compare_ops_vk[size_t(s.compare)];
    sampler_info.mipmapMode = g_mipmap_mode_vk[size_t(s.filter)];
    sampler_info.mipLodBias = s.lod_bias.to_float();
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;

    const VkResult res = api_ctx_->vkCreateSampler(api_ctx_->device, &sampler_info, nullptr, &handle_.sampler);
    assert(res == VK_SUCCESS && "Failed to create sampler!");

    params.sampling = s;
}

void Ren::CopyImageToImage(CommandBuffer cmd_buf, const Texture &src_tex, const uint32_t src_level, const uint32_t src_x,
                           const uint32_t src_y, const uint32_t src_z, Texture &dst_tex, const uint32_t dst_level,
                           const uint32_t dst_x, const uint32_t dst_y, const uint32_t dst_z, const uint32_t dst_face,
                           const uint32_t w, const uint32_t h, const uint32_t d) {
    assert(src_tex.resource_state == eResState::CopySrc);
    assert(dst_tex.resource_state == eResState::CopyDst);

    VkImageCopy reg;
    if (IsDepthFormat(src_tex.params.format)) {
        reg.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        reg.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    reg.srcSubresource.baseArrayLayer = 0;
    reg.srcSubresource.layerCount = 1;
    reg.srcSubresource.mipLevel = src_level;
    reg.srcOffset = {int32_t(src_x), int32_t(src_y), int32_t(src_z)};
    if (IsDepthFormat(dst_tex.params.format)) {
        reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    reg.dstSubresource.baseArrayLayer = dst_face;
    reg.dstSubresource.layerCount = 1;
    reg.dstSubresource.mipLevel = dst_level;
    reg.dstOffset = {int32_t(dst_x), int32_t(dst_y), int32_t(dst_z)};
    reg.extent = {w, h, d};

    src_tex.api_ctx()->vkCmdCopyImage(cmd_buf, src_tex.handle().img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                      dst_tex.handle().img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
}

void Ren::ClearImage(Texture &tex, const ClearColor &col, CommandBuffer cmd_buf) {
    assert(tex.resource_state == eResState::CopyDst);

    VkImageSubresourceRange clear_range = {};
    clear_range.baseMipLevel = 0;
    clear_range.levelCount = VK_REMAINING_MIP_LEVELS;
    clear_range.baseArrayLayer = 0;
    clear_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    if (!IsDepthFormat(tex.params.format)) {
        clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkClearColorValue clear_val = {};
        memcpy(clear_val.uint32, col.uint32, 4 * sizeof(float));

        tex.api_ctx()->vkCmdClearColorImage(cmd_buf, tex.handle().img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val,
                                            1, &clear_range);
    } else {
        clear_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        VkClearDepthStencilValue clear_val = {};
        clear_val.depth = col.float32[0];

        tex.api_ctx()->vkCmdClearDepthStencilImage(cmd_buf, tex.handle().img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   &clear_val, 1, &clear_range);
    }
}

////////////////////////////////////////////////////////////////////////////////////////

VkFormat Ren::VKFormatFromTexFormat(eTexFormat format) { return g_formats_vk[size_t(format)]; }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
