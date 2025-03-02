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
extern const VkFormat g_vk_formats[] = {
#include "TextureFormat.inl"
};
#undef X

uint32_t TextureHandleCounter = 0;

// make sure we can simply cast these
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT == 1, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT == 2, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT == 4, "!");
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT == 8, "!");

VkFormat ToSRGBFormat(const VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8_UNORM:
        return VK_FORMAT_R8G8B8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case VK_FORMAT_BC2_UNORM_BLOCK:
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case VK_FORMAT_BC3_UNORM_BLOCK:
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
    default:
        assert(false && "Unsupported format!");
    }
    return VK_FORMAT_UNDEFINED;
}

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
int GetMipDataLenBytes(int w, int h, eTexFormat format);
void ParseDDSHeader(const DDSHeader &hdr, Tex2DParams *params);

extern const VkFilter g_vk_min_mag_filter[];
extern const VkSamplerAddressMode g_vk_wrap_mode[];
extern const VkSamplerMipmapMode g_vk_mipmap_mode[];
extern const VkCompareOp g_vk_compare_ops[];

extern const float AnisotropyLevel;
} // namespace Ren

Ren::Texture2D::Texture2D(std::string_view name, ApiContext *api_ctx, const Tex2DParams &p, MemAllocators *mem_allocs,
                          ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(p, mem_allocs, log);
}

Ren::Texture2D::Texture2D(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data, const Tex2DParams &p,
                          MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(data, p, mem_allocs, load_status, log);
}

Ren::Texture2D::Texture2D(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data[6], const Tex2DParams &p,
                          MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log)
    : api_ctx_(api_ctx), name_(name) {
    Init(data, p, mem_allocs, load_status, log);
}

Ren::Texture2D::~Texture2D() { Free(); }

Ren::Texture2D &Ren::Texture2D::operator=(Texture2D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    alloc_ = std::exchange(rhs.alloc_, {});
    initialized_mips_ = std::exchange(rhs.initialized_mips_, 0);
    params = std::exchange(rhs.params, {});
    ready_ = std::exchange(rhs.ready_, false);
    name_ = std::move(rhs.name_);

    resource_state = std::exchange(rhs.resource_state, eResState::Undefined);

    return (*this);
}

void Ren::Texture2D::Init(const Tex2DParams &p, MemAllocators *mem_allocs, ILog *log) {
    InitFromRAWData(nullptr, 0, nullptr, mem_allocs, p, log);
    ready_ = true;
}

void Ren::Texture2D::Init(const TexHandle &handle, const Tex2DParams &_params, MemAllocation &&alloc, ILog *log) {
    handle_ = handle;
    alloc_ = std::move(alloc);
    params = _params;

    if (handle.views[0] == VkImageView{} && params.usage != Bitmask(eTexUsage::Transfer) &&
        !bool(params.flags & eTexFlags::NoOwnership)) {
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = handle_.img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = g_vk_formats[size_t(params.format)];
        if (bool(params.flags & eTexFlags::SRGB)) {
            view_info.format = ToSRGBFormat(view_info.format);
        }
        if (IsDepthStencilFormat(params.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        } else if (IsDepthFormat(params.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        } else {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = params.mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views[0]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }

        if (IsDepthStencilFormat(params.format)) {
            // create additional depth-only image view
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            VkImageView depth_only_view;
            const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &depth_only_view);
            if (res != VK_SUCCESS) {
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

        if (params.flags & eTexFlags::ExtendedViews) {
            // create additional image views
            for (int j = 0; j < params.mip_count; ++j) {
                VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                view_info.image = handle_.img;
                view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                view_info.format = g_vk_formats[size_t(params.format)];
                if (params.flags & eTexFlags::SRGB) {
                    view_info.format = ToSRGBFormat(view_info.format);
                }
                view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                view_info.subresourceRange.baseMipLevel = j;
                view_info.subresourceRange.levelCount = 1;
                view_info.subresourceRange.baseArrayLayer = 0;
                view_info.subresourceRange.layerCount = 1;

                handle_.views.emplace_back(VK_NULL_HANDLE);
                const VkResult res =
                    api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views.back());
                if (res != VK_SUCCESS) {
                    log->Error("Failed to create image view!");
                    return;
                }

#ifdef ENABLE_GPU_DEBUG
                VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
                name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
                name_info.objectHandle = uint64_t(handle_.views.back());
                name_info.pObjectName = name_.c_str();
                api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif
            }
        }
    }

    if (handle_.sampler == VkSampler{} && !bool(params.flags & eTexFlags::NoOwnership)) {
        SetSampling(params.sampling);
    }

    ready_ = true;
}

void Ren::Texture2D::Init(Span<const uint8_t> data, const Tex2DParams &p, MemAllocators *mem_allocs,
                          eTexLoadStatus *load_status, ILog *log) {
    if (data.empty()) {
        auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload, 4};
        { // Update staging buffer
            uint8_t *stage_data = sbuf.Map();
            memcpy(stage_data, p.fallback_color, 4);
            sbuf.Unmap();
        }

        CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();

        Tex2DParams _p = p;
        _p.w = _p.h = 1;
        _p.mip_count = 1;
        _p.format = eTexFormat::RGBA8;
        _p.usage = Bitmask(eTexUsage::Sampled) | eTexUsage::Transfer;

        InitFromRAWData(&sbuf, 0, cmd_buf, mem_allocs, _p, log);

        api_ctx_->EndSingleTimeCommands(cmd_buf);
        sbuf.FreeImmediate();

        // mark it as not ready
        ready_ = false;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, mem_allocs, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, mem_allocs, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, mem_allocs, p, log);
        } else {
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
        }
        ready_ = true;
        (*load_status) = eTexLoadStatus::CreatedFromData;
    }
}

void Ren::Texture2D::Init(Span<const uint8_t> data[6], const Tex2DParams &p, MemAllocators *mem_allocs,
                          eTexLoadStatus *load_status, ILog *log) {
    if (!data) {
        auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload, 4};
        { // Update staging buffer
            uint8_t *stage_data = sbuf.Map();
            memcpy(stage_data, p.fallback_color, 4);
            sbuf.Unmap();
        }

        CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();

        Tex2DParams _p = p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RGBA8;
        _p.usage = Bitmask(eTexUsage::Sampled) | eTexUsage::Transfer;

        int data_off[6] = {};
        InitFromRAWData(sbuf, data_off, cmd_buf, mem_allocs, _p, log);

        api_ctx_->EndSingleTimeCommands(cmd_buf);
        sbuf.FreeImmediate();

        // mark it as not ready
        ready_ = false;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, mem_allocs, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, mem_allocs, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, mem_allocs, p, log);
        } else {
            auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload,
                               uint32_t(data[0].size() + data[1].size() + data[2].size() + data[3].size() +
                                        data[4].size() + data[5].size())};
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
        }

        ready_ = true;
        (*load_status) = eTexLoadStatus::CreatedFromData;
    }
}

void Ren::Texture2D::Free() {
    if (params.format != eTexFormat::Undefined && !(params.flags & eTexFlags::NoOwnership)) {
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
        ready_ = false;
    }
}

void Ren::Texture2D::FreeImmediate() {
    if (params.format != eTexFormat::Undefined && !(params.flags & eTexFlags::NoOwnership)) {
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
        ready_ = false;
    }
}

bool Ren::Texture2D::Realloc(const int w, const int h, int mip_count, const int samples, const eTexFormat format,
                             const bool is_srgb, CommandBuffer cmd_buf, MemAllocators *mem_allocs, ILog *log) {
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
        img_info.format = g_vk_formats[size_t(format)];
        if (is_srgb) {
            img_info.format = ToSRGBFormat(img_info.format);
        }
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        assert(uint8_t(params.usage) != 0);
        img_info.usage = to_vk_image_usage(params.usage, format);

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
        view_info.format = g_vk_formats[size_t(format)];
        if (is_srgb) {
            view_info.format = ToSRGBFormat(view_info.format);
        }
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

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
    uint16_t new_initialized_mips = 0;

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
            if (initialized_mips_ & (1u << src_mip)) {
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

                new_initialized_mips |= (1u << dst_mip);
            }
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
                new_barrier.subresourceRange.levelCount = params.mip_count; // transit the whole image
                new_barrier.subresourceRange.baseArrayLayer = 0;
                new_barrier.subresourceRange.layerCount = 1;

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
                new_barrier.subresourceRange.levelCount = mip_count; // transit the whole image
                new_barrier.subresourceRange.baseArrayLayer = 0;
                new_barrier.subresourceRange.layerCount = 1;

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
    if (is_srgb) {
        params.flags |= eTexFlags::SRGB;
    } else {
        params.flags &= ~Bitmask(eTexFlags::SRGB);
    }
    params.mip_count = mip_count;
    params.samples = samples;
    params.format = format;
    initialized_mips_ = new_initialized_mips;

    if (params.flags & eTexFlags::ExtendedViews) {
        // create additional image views
        for (int j = 0; j < mip_count; ++j) {
            VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view_info.image = handle_.img;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = g_vk_formats[size_t(params.format)];
            if (params.flags & eTexFlags::SRGB) {
                view_info.format = ToSRGBFormat(view_info.format);
            }
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = j;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            handle_.views.emplace_back(VK_NULL_HANDLE);
            const VkResult res =
                api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views.back());
            if (res != VK_SUCCESS) {
                log->Error("Failed to create image view!");
                return false;
            }

#ifdef ENABLE_GPU_DEBUG
            VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
            name_info.objectHandle = uint64_t(handle_.views.back());
            name_info.pObjectName = name_.c_str();
            api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif
        }
    }

    this->resource_state = new_resource_state;

    return true;
}

void Ren::Texture2D::InitFromRAWData(Buffer *sbuf, int data_off, CommandBuffer cmd_buf, MemAllocators *mem_allocs,
                                     const Tex2DParams &p, ILog *log) {
    Free();

    handle_.generation = TextureHandleCounter++;
    params = p;
    initialized_mips_ = 0;

    int mip_count = params.mip_count;
    if (!mip_count) {
        mip_count = CalcMipCount(p.w, p.h, 1);
    }

    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count;
        img_info.arrayLayers = 1;
        img_info.format = g_vk_formats[size_t(p.format)];
        if (p.flags & eTexFlags::SRGB) {
            img_info.format = ToSRGBFormat(img_info.format);
        }
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
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = g_vk_formats[size_t(p.format)];
        if (p.flags & eTexFlags::SRGB) {
            view_info.format = ToSRGBFormat(view_info.format);
        }
        if (IsDepthStencilFormat(p.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        } else if (IsDepthFormat(p.format)) {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        } else {
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views[0]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
        }

        if (IsDepthStencilFormat(p.format)) {
            // create additional depth-only image view
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            VkImageView depth_only_view;
            const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &depth_only_view);
            if (res != VK_SUCCESS) {
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

        if (params.flags & eTexFlags::ExtendedViews) {
            // create additional image views
            for (int j = 0; j < mip_count; ++j) {
                VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                view_info.image = handle_.img;
                view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                view_info.format = g_vk_formats[size_t(p.format)];
                if (p.flags & eTexFlags::SRGB) {
                    view_info.format = ToSRGBFormat(view_info.format);
                }
                view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                view_info.subresourceRange.baseMipLevel = j;
                view_info.subresourceRange.levelCount = 1;
                view_info.subresourceRange.baseArrayLayer = 0;
                view_info.subresourceRange.layerCount = 1;

                handle_.views.emplace_back(VK_NULL_HANDLE);
                const VkResult res =
                    api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views.back());
                if (res != VK_SUCCESS) {
                    log->Error("Failed to create image view!");
                    return;
                }

#ifdef ENABLE_GPU_DEBUG
                VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
                name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
                name_info.objectHandle = uint64_t(handle_.views.back());
                name_info.pObjectName = name_.c_str();
                api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif
            }
        }
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
            new_barrier.subresourceRange.levelCount = mip_count; // transit whole image
            new_barrier.subresourceRange.baseArrayLayer = 0;
            new_barrier.subresourceRange.layerCount = 1;

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

        VkBufferImageCopy region = {};
        region.bufferOffset = VkDeviceSize(data_off);
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {uint32_t(p.w), uint32_t(p.h), 1};

        api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf->vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         1, &region);

        initialized_mips_ |= (1u << 0);
    }

    { // create new sampler
        VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler_info.magFilter = g_vk_min_mag_filter[size_t(p.sampling.filter)];
        sampler_info.minFilter = g_vk_min_mag_filter[size_t(p.sampling.filter)];
        sampler_info.addressModeU = g_vk_wrap_mode[size_t(p.sampling.wrap)];
        sampler_info.addressModeV = g_vk_wrap_mode[size_t(p.sampling.wrap)];
        sampler_info.addressModeW = g_vk_wrap_mode[size_t(p.sampling.wrap)];
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = AnisotropyLevel;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = p.sampling.compare != eTexCompare::None ? VK_TRUE : VK_FALSE;
        sampler_info.compareOp = g_vk_compare_ops[size_t(p.sampling.compare)];
        sampler_info.mipmapMode = g_vk_mipmap_mode[size_t(p.sampling.filter)];
        sampler_info.mipLodBias = p.sampling.lod_bias.to_float();
        sampler_info.minLod = p.sampling.min_lod.to_float();
        sampler_info.maxLod = p.sampling.max_lod.to_float();

        const VkResult res = api_ctx_->vkCreateSampler(api_ctx_->device, &sampler_info, nullptr, &handle_.sampler);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create sampler!");
        }
    }
}

void Ren::Texture2D::InitFromTGAFile(Span<const uint8_t> data, MemAllocators *mem_allocs, const Tex2DParams &p,
                                     ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    uint32_t img_size = 0;
    const bool res1 = ReadTGAFile(data, w, h, format, nullptr, img_size);
    if (!res1) {
        return;
    }

    auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload, img_size};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        const bool res2 = ReadTGAFile(data, w, h, format, stage_data, img_size);
        assert(res2);
        sbuf.Unmap();
    }

    CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(&sbuf, 0, cmd_buf, mem_allocs, _p, log);

    api_ctx_->EndSingleTimeCommands(cmd_buf);
    sbuf.FreeImmediate();
}

void Ren::Texture2D::InitFromDDSFile(Span<const uint8_t> data, MemAllocators *mem_allocs, const Tex2DParams &p,
                                     ILog *log) {
    Free();

    int bytes_left = int(data.size());
    const uint8_t *p_data = data.data();

    if (bytes_left < sizeof(DDSHeader)) {
        log->Error("Failed to parse DDS header!");
        return;
    }

    DDSHeader header;
    memcpy(&header, p_data, sizeof(DDSHeader));
    p_data += sizeof(DDSHeader);
    bytes_left -= sizeof(DDSHeader);

    Tex2DParams _p = p;
    ParseDDSHeader(header, &_p);

    if (header.sPixelFormat.dwFourCC ==
        ((unsigned('D') << 0u) | (unsigned('X') << 8u) | (unsigned('1') << 16u) | (unsigned('0') << 24u))) {
        if (bytes_left < sizeof(DDS_HEADER_DXT10)) {
            log->Error("Failed to parse DDS header!");
            return;
        }

        DDS_HEADER_DXT10 dx10_header = {};
        memcpy(&dx10_header, p_data, sizeof(DDS_HEADER_DXT10));
        _p.format = TexFormatFromDXGIFormat(dx10_header.dxgiFormat);

        p_data += sizeof(DDS_HEADER_DXT10);
        bytes_left -= sizeof(DDS_HEADER_DXT10);
    } else if (_p.format == eTexFormat::Undefined) {
        // Try to use least significant bits of FourCC as format
        const uint8_t val = (header.sPixelFormat.dwFourCC & 0xff);
        if (val == 0x6f) {
            _p.format = eTexFormat::R16F;
        } else if (val == 0x70) {
            _p.format = eTexFormat::RG16F;
        } else if (val == 0x71) {
            _p.format = eTexFormat::RGBA16F;
        } else if (val == 0x72) {
            _p.format = eTexFormat::R32F;
        } else if (val == 0x73) {
            _p.format = eTexFormat::RG32F;
        } else if (val == 0x74) {
            _p.format = eTexFormat::RGBA32F;
        } else if (val == 0) {
            if (header.sPixelFormat.dwRGBBitCount == 8) {
                _p.format = eTexFormat::R8;
            } else if (header.sPixelFormat.dwRGBBitCount == 16) {
                _p.format = eTexFormat::RG8;
                assert(header.sPixelFormat.dwRBitMask == 0x00ff);
                assert(header.sPixelFormat.dwGBitMask == 0xff00);
            }
        }
    }

    if (_p.format == eTexFormat::Undefined) {
        log->Error("Failed to parse DDS header!");
        return;
    }

    params.usage = _p.usage;
    params.flags = _p.flags;
    params.sampling = _p.sampling;

    auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload, uint32_t(bytes_left)};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        memcpy(stage_data, p_data, bytes_left);
        sbuf.Unmap();
    }

    CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();

    Realloc(_p.w, _p.h, _p.mip_count, 1, _p.format, (_p.flags & eTexFlags::SRGB), cmd_buf, mem_allocs, log);

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
        new_barrier.offset = 0;
        new_barrier.size = VkDeviceSize(bytes_left);

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
        new_barrier.subresourceRange.levelCount = params.mip_count; // transit the whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = 1;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                       uint32_t(img_barriers.size()), img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy regions[16] = {};
    int regions_count = 0;

    int w = params.w, h = params.h;
    uintptr_t data_off = 0;
    for (uint32_t i = 0; i < params.mip_count; i++) {
        const int len = GetMipDataLenBytes(w, h, params.format);
        if (len > bytes_left) {
            log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
            return;
        }

        VkBufferImageCopy &reg = regions[regions_count++];

        reg.bufferOffset = VkDeviceSize(data_off);
        reg.bufferRowLength = 0;
        reg.bufferImageHeight = 0;

        reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        reg.imageSubresource.mipLevel = i;
        reg.imageSubresource.baseArrayLayer = 0;
        reg.imageSubresource.layerCount = 1;

        reg.imageOffset = {0, 0, 0};
        reg.imageExtent = {uint32_t(w), uint32_t(h), 1};

        initialized_mips_ |= (1u << i);

        data_off += len;
        bytes_left -= len;
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     regions_count, regions);

    api_ctx_->EndSingleTimeCommands(cmd_buf);
    sbuf.FreeImmediate();

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromKTXFile(Span<const uint8_t> data, MemAllocators *mem_allocs, const Tex2DParams &p,
                                     ILog *log) {
    KTXHeader header;
    memcpy(&header, data.data(), sizeof(KTXHeader));

    bool is_srgb_format;
    eTexFormat format = FormatFromGLInternalFormat(header.gl_internal_format, &is_srgb_format);

    if (is_srgb_format && !(params.flags & eTexFlags::SRGB)) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    Free();
    Realloc(int(header.pixel_width), int(header.pixel_height), int(header.mipmap_levels_count), 1, format,
            (p.flags & eTexFlags::SRGB), nullptr, nullptr, log);

    params.flags = p.flags;
    params.sampling = p.sampling;

    int w = int(params.w);
    int h = int(params.h);

    params.w = w;
    params.h = h;

    int data_offset = sizeof(KTXHeader);

    auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload, uint32_t(data.size() - data_offset)};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        memcpy(stage_data, data.data(), data.size() - data_offset);
        sbuf.Unmap();
    }

    CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();

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
        new_barrier.offset = 0;
        new_barrier.size = VkDeviceSize(data.size());

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
        new_barrier.subresourceRange.levelCount = params.mip_count; // transit the whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = 1;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                       uint32_t(img_barriers.size()), img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy regions[16] = {};
    int regions_count = 0;

    for (int i = 0; i < int(header.mipmap_levels_count); i++) {
        if (data_offset + int(sizeof(uint32_t)) > data.size()) {
            log->Error("Insufficient data length, bytes left %i, expected %i", int(data.size() - data_offset),
                       int(sizeof(uint32_t)));
            break;
        }

        uint32_t img_size;
        memcpy(&img_size, &data[data_offset], sizeof(uint32_t));
        if (data_offset + int(img_size) > data.size()) {
            log->Error("Insufficient data length, bytes left %i, expected %i", int(data.size() - data_offset),
                       img_size);
            break;
        }

        data_offset += sizeof(uint32_t);

        VkBufferImageCopy &reg = regions[regions_count++];

        reg.bufferOffset = VkDeviceSize(data_offset);
        reg.bufferRowLength = 0;
        reg.bufferImageHeight = 0;

        reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        reg.imageSubresource.mipLevel = i;
        reg.imageSubresource.baseArrayLayer = 0;
        reg.imageSubresource.layerCount = 1;

        reg.imageOffset = {0, 0, 0};
        reg.imageExtent = {uint32_t(w), uint32_t(h), 1};

        initialized_mips_ |= (1u << i);
        data_offset += img_size;

        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);

        const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
        data_offset += pad;
    }

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     regions_count, regions);

    api_ctx_->EndSingleTimeCommands(cmd_buf);
    sbuf.FreeImmediate();

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromRAWData(Buffer &sbuf, int data_off[6], CommandBuffer cmd_buf, MemAllocators *mem_allocs,
                                     const Tex2DParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    handle_.generation = TextureHandleCounter++;
    params = p;
    initialized_mips_ = 0;

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
        img_info.format = g_vk_formats[size_t(p.format)];
        if (p.flags & eTexFlags::SRGB) {
            img_info.format = ToSRGBFormat(img_info.format);
        }
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
        view_info.format = g_vk_formats[size_t(p.format)];
        if (p.flags & eTexFlags::SRGB) {
            view_info.format = ToSRGBFormat(view_info.format);
        }
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 6;

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

    if (params.flags & eTexFlags::ExtendedViews) {
        // create additional image views
        for (int j = 0; j < mip_count; ++j) {
            for (int i = 0; i < 6; ++i) {
                VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
                view_info.image = handle_.img;
                view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                view_info.format = g_vk_formats[size_t(p.format)];
                if (p.flags & eTexFlags::SRGB) {
                    view_info.format = ToSRGBFormat(view_info.format);
                }
                view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                view_info.subresourceRange.baseMipLevel = j;
                view_info.subresourceRange.levelCount = 1;
                view_info.subresourceRange.baseArrayLayer = i;
                view_info.subresourceRange.layerCount = 1;

                handle_.views.emplace_back(VK_NULL_HANDLE);
                const VkResult res =
                    api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views.back());
                if (res != VK_SUCCESS) {
                    log->Error("Failed to create image view!");
                    return;
                }

#ifdef ENABLE_GPU_DEBUG
                VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
                name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
                name_info.objectHandle = uint64_t(handle_.views.back());
                name_info.pObjectName = name_.c_str();
                api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif
            }
        }
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
        new_barrier.subresourceRange.levelCount = mip_count; // transit whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = 6;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                       uint32_t(img_barriers.size()), img_barriers.cdata());
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

            buffer_offset += GetMipDataLenBytes(int(reg.imageExtent.width), int(reg.imageExtent.height), p.format);
        }
    }

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     uint32_t(regions.size()), regions.data());

    initialized_mips_ |= (1u << 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromTGAFile(Span<const uint8_t> data[6], MemAllocators *mem_allocs, const Tex2DParams &p,
                                     ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;

    auto sbuf = Buffer{
        "Temp Stage Buf", api_ctx_, eBufType::Upload,
        uint32_t(data[0].size() + data[1].size() + data[2].size() + data[3].size() + data[4].size() + data[5].size())};
    int data_off[6] = {-1, -1, -1, -1, -1, -1};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        uint32_t stage_off = 0;

        for (int i = 0; i < 6; i++) {
            if (!data[i].empty()) {
                uint32_t data_size;
                const bool res1 = ReadTGAFile(data[i], w, h, format, nullptr, data_size);
                assert(res1);

                assert(stage_off + data_size < sbuf.size());
                const bool res2 = ReadTGAFile(data[i], w, h, format, &stage_data[stage_off], data_size);
                assert(res2);

                data_off[i] = int(stage_off);
                stage_off += data_size;
            }
        }
        sbuf.Unmap();
    }

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();
    InitFromRAWData(sbuf, data_off, cmd_buf, mem_allocs, _p, log);
    api_ctx_->EndSingleTimeCommands(cmd_buf);
    sbuf.FreeImmediate();
}

void Ren::Texture2D::InitFromDDSFile(Span<const uint8_t> data[6], MemAllocators *mem_allocs, const Tex2DParams &p,
                                     ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    uint32_t data_off[6] = {};
    uint32_t stage_len = 0;

    eTexFormat first_format = eTexFormat::Undefined;
    uint32_t first_mip_count = 0;
    int first_block_size_bytes = 0;

    for (int i = 0; i < 6; ++i) {
        const DDSHeader *header = reinterpret_cast<const DDSHeader *>(data[i].data());

        eTexFormat format;
        int block_size_bytes;
        const int px_format = int(header->sPixelFormat.dwFourCC >> 24u) - '0';
        switch (px_format) {
        case 1:
            format = eTexFormat::BC1;
            block_size_bytes = 8;
            break;
        case 3:
            format = eTexFormat::BC2;
            block_size_bytes = 16;
            break;
        case 5:
            format = eTexFormat::BC3;
            block_size_bytes = 16;
            break;
        default:
            log->Error("Unknow DDS pixel format %i", px_format);
            return;
        }

        if (i == 0) {
            first_format = format;
            first_mip_count = header->dwMipMapCount;
            first_block_size_bytes = block_size_bytes;
        } else {
            assert(format == first_format);
            assert(first_mip_count == header->dwMipMapCount);
            assert(block_size_bytes == first_block_size_bytes);
        }

        data_off[i] = stage_len;
        stage_len += uint32_t(data[i].size());
    }

    auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload, stage_len};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        for (int i = 0; i < 6; ++i) {
            memcpy(stage_data + data_off[i], data[i].data(), data[i].size());
        }
        sbuf.Unmap();
    }

    handle_.generation = TextureHandleCounter++;
    params = p;
    params.cube = 1;
    initialized_mips_ = 0;

    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = 1;
        img_info.mipLevels = first_mip_count;
        img_info.arrayLayers = 6;
        img_info.format = g_vk_formats[size_t(first_format)];
        if (p.flags & eTexFlags::SRGB) {
            img_info.format = ToSRGBFormat(img_info.format);
        }
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        assert(uint8_t(p.usage) != 0);
        img_info.usage = to_vk_image_usage(p.usage, first_format);
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VK_SAMPLE_COUNT_1_BIT;
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
        view_info.format = g_vk_formats[size_t(p.format)];
        if (p.flags & eTexFlags::SRGB) {
            view_info.format = ToSRGBFormat(view_info.format);
        }
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = first_mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 6;

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

    CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();

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
        new_barrier.subresourceRange.levelCount = first_mip_count; // transit whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = 6;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                       uint32_t(img_barriers.size()), img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy regions[6 * 16] = {};
    int regions_count = 0;

    for (int i = 0; i < 6; i++) {
        const auto *header = reinterpret_cast<const DDSHeader *>(data[i].data());

        int offset = sizeof(DDSHeader);
        int data_len = int(data[i].size() - sizeof(DDSHeader));

        for (uint32_t j = 0; j < header->dwMipMapCount; j++) {
            const int width = std::max(int(header->dwWidth >> j), 1), height = std::max(int(header->dwHeight >> j), 1);

            const int image_len = ((width + 3) / 4) * ((height + 3) / 4) * first_block_size_bytes;
            if (image_len > data_len) {
                log->Error("Insufficient data length, bytes left %i, expected %i", data_len, image_len);
                break;
            }

            auto &reg = regions[regions_count++];

            reg.bufferOffset = VkDeviceSize(data_off[i] + offset);
            reg.bufferRowLength = 0;
            reg.bufferImageHeight = 0;

            reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            reg.imageSubresource.mipLevel = uint32_t(j);
            reg.imageSubresource.baseArrayLayer = i;
            reg.imageSubresource.layerCount = 1;

            reg.imageOffset = {0, 0, 0};
            reg.imageExtent = {uint32_t(width), uint32_t(height), 1};

            offset += image_len;
            data_len -= image_len;
        }
    }

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     regions_count, regions);

    api_ctx_->EndSingleTimeCommands(cmd_buf);
    sbuf.FreeImmediate();

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromKTXFile(Span<const uint8_t> data[6], MemAllocators *mem_allocs, const Tex2DParams &p,
                                     ILog *log) {
    Free();

    const auto *first_header = reinterpret_cast<const KTXHeader *>(data[0].data());

    uint32_t data_off[6] = {};
    uint32_t stage_len = 0;

    for (int i = 0; i < 6; ++i) {
        const auto *this_header = reinterpret_cast<const KTXHeader *>(data[i].data());

        // make sure all images have same properties
        if (this_header->pixel_width != first_header->pixel_width) {
            log->Error("Image width mismatch %i, expected %i", int(this_header->pixel_width),
                       int(first_header->pixel_width));
            continue;
        }
        if (this_header->pixel_height != first_header->pixel_height) {
            log->Error("Image height mismatch %i, expected %i", int(this_header->pixel_height),
                       int(first_header->pixel_height));
            continue;
        }
        if (this_header->gl_internal_format != first_header->gl_internal_format) {
            log->Error("Internal format mismatch %i, expected %i", int(this_header->gl_internal_format),
                       int(first_header->gl_internal_format));
            continue;
        }

        data_off[i] = stage_len;
        stage_len += uint32_t(data[i].size());
    }

    auto sbuf = Buffer{"Temp Stage Buf", api_ctx_, eBufType::Upload, stage_len};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        for (int i = 0; i < 6; ++i) {
            memcpy(stage_data + data_off[i], data[i].data(), data[i].size());
        }
        sbuf.Unmap();
    }

    handle_.generation = TextureHandleCounter++;
    params = p;
    params.cube = 1;
    initialized_mips_ = 0;

    bool is_srgb_format;
    params.format = FormatFromGLInternalFormat(first_header->gl_internal_format, &is_srgb_format);

    if (is_srgb_format && !(params.flags & eTexFlags::SRGB)) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = 1;
        img_info.mipLevels = first_header->mipmap_levels_count;
        img_info.arrayLayers = 6;
        img_info.format = g_vk_formats[size_t(params.format)];
        if (params.flags & eTexFlags::SRGB) {
            img_info.format = ToSRGBFormat(img_info.format);
        }
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        assert(uint8_t(p.usage) != 0);
        img_info.usage = to_vk_image_usage(p.usage, params.format);
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VK_SAMPLE_COUNT_1_BIT;
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
        view_info.format = g_vk_formats[size_t(p.format)];
        if (p.flags & eTexFlags::SRGB) {
            view_info.format = ToSRGBFormat(view_info.format);
        }
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = first_header->mipmap_levels_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 6;

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

    CommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();

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
        new_barrier.subresourceRange.levelCount = first_header->mipmap_levels_count; // transit whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = 6;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                       uint32_t(img_barriers.size()), img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy regions[6 * 16] = {};
    int regions_count = 0;

    for (int i = 0; i < 6; ++i) {
#ifndef NDEBUG
        const auto *this_header = reinterpret_cast<const KTXHeader *>(data[i].data());

        // make sure all images have same properties
        if (this_header->pixel_width != first_header->pixel_width) {
            log->Error("Image width mismatch %i, expected %i", int(this_header->pixel_width),
                       int(first_header->pixel_width));
            continue;
        }
        if (this_header->pixel_height != first_header->pixel_height) {
            log->Error("Image height mismatch %i, expected %i", int(this_header->pixel_height),
                       int(first_header->pixel_height));
            continue;
        }
        if (this_header->gl_internal_format != first_header->gl_internal_format) {
            log->Error("Internal format mismatch %i, expected %i", int(this_header->gl_internal_format),
                       int(first_header->gl_internal_format));
            continue;
        }
#endif
        int data_offset = sizeof(KTXHeader);
        int _w = params.w, _h = params.h;

        for (int j = 0; j < int(first_header->mipmap_levels_count); j++) {
            uint32_t img_size;
            memcpy(&img_size, &data[data_offset], sizeof(uint32_t));
            data_offset += sizeof(uint32_t);

            auto &reg = regions[regions_count++];

            reg.bufferOffset = VkDeviceSize(data_off[i] + data_offset);
            reg.bufferRowLength = 0;
            reg.bufferImageHeight = 0;

            reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            reg.imageSubresource.mipLevel = uint32_t(j);
            reg.imageSubresource.baseArrayLayer = i;
            reg.imageSubresource.layerCount = 1;

            reg.imageOffset = {0, 0, 0};
            reg.imageExtent = {uint32_t(_w), uint32_t(_h), 1};

            data_offset += img_size;

            _w = std::max(_w / 2, 1);
            _h = std::max(_h / 2, 1);

            const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
            data_offset += pad;
        }
    }

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     regions_count, regions);

    api_ctx_->EndSingleTimeCommands(cmd_buf);
    sbuf.FreeImmediate();

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::SetSubImage(const int level, const int offsetx, const int offsety, const int sizex,
                                 const int sizey, const eTexFormat format, const Buffer &sbuf, CommandBuffer cmd_buf,
                                 const int data_off, const int data_len) {
    assert(format == params.format);
    assert(params.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params.h >> level, 1));

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
        new_barrier.subresourceRange.levelCount = params.mip_count; // transit whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = 1;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                       uint32_t(img_barriers.size()), img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy region = {};

    region.bufferOffset = VkDeviceSize(data_off);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = uint32_t(level);
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {int32_t(offsetx), int32_t(offsety), 0};
    region.imageExtent = {uint32_t(sizex), uint32_t(sizey), 1};

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                     &region);

    if (offsetx == 0 && offsety == 0 && sizex == std::max(params.w >> level, 1) &&
        sizey == std::max(params.h >> level, 1)) {
        // consider this level initialized
        initialized_mips_ |= (1u << level);
    }
}

void Ren::Texture2D::SetSampling(const SamplingParams s) {
    if (handle_.sampler) {
        api_ctx_->samplers_to_destroy[api_ctx_->backend_frame].emplace_back(handle_.sampler);
    }

    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = g_vk_min_mag_filter[size_t(s.filter)];
    sampler_info.minFilter = g_vk_min_mag_filter[size_t(s.filter)];
    sampler_info.addressModeU = g_vk_wrap_mode[size_t(s.wrap)];
    sampler_info.addressModeV = g_vk_wrap_mode[size_t(s.wrap)];
    sampler_info.addressModeW = g_vk_wrap_mode[size_t(s.wrap)];
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = AnisotropyLevel;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = s.compare != eTexCompare::None ? VK_TRUE : VK_FALSE;
    sampler_info.compareOp = g_vk_compare_ops[size_t(s.compare)];
    sampler_info.mipmapMode = g_vk_mipmap_mode[size_t(s.filter)];
    sampler_info.mipLodBias = s.lod_bias.to_float();
    sampler_info.minLod = s.min_lod.to_float();
    sampler_info.maxLod = s.max_lod.to_float();

    const VkResult res = api_ctx_->vkCreateSampler(api_ctx_->device, &sampler_info, nullptr, &handle_.sampler);
    assert(res == VK_SUCCESS && "Failed to create sampler!");

    params.sampling = s;
}

void Ren::Texture2D::CopyTextureData(const Buffer &sbuf, CommandBuffer cmd_buf, const int data_off,
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
        new_barrier.subresourceRange.levelCount = params.mip_count; // transit whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = 1;

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
                                       0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                       uint32_t(img_barriers.size()), img_barriers.cdata());
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

void Ren::CopyImageToImage(CommandBuffer cmd_buf, Texture2D &src_tex, const uint32_t src_level, const uint32_t src_x,
                           const uint32_t src_y, Texture2D &dst_tex, const uint32_t dst_level, const uint32_t dst_x,
                           const uint32_t dst_y, const uint32_t dst_face, const uint32_t width, const uint32_t height) {
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
    reg.srcOffset = {int32_t(src_x), int32_t(src_y), 0};
    if (IsDepthFormat(dst_tex.params.format)) {
        reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    reg.dstSubresource.baseArrayLayer = dst_face;
    reg.dstSubresource.layerCount = 1;
    reg.dstSubresource.mipLevel = dst_level;
    reg.dstOffset = {int32_t(dst_x), int32_t(dst_y), 0};
    reg.extent = {width, height, 1};

    src_tex.api_ctx()->vkCmdCopyImage(cmd_buf, src_tex.handle().img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                      dst_tex.handle().img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
}

void Ren::ClearImage(Texture2D &tex, const float rgba[4], CommandBuffer cmd_buf) {
    assert(tex.resource_state == eResState::CopyDst);

    if (!IsDepthFormat(tex.params.format)) {
        VkClearColorValue clear_val = {};
        memcpy(clear_val.float32, rgba, 4 * sizeof(float));

        VkImageSubresourceRange clear_range = {};
        clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clear_range.layerCount = 1;
        clear_range.levelCount = 1;

        tex.api_ctx()->vkCmdClearColorImage(cmd_buf, tex.handle().img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val,
                                            1, &clear_range);
    } else {
        VkClearDepthStencilValue clear_val = {};
        clear_val.depth = rgba[0];

        VkImageSubresourceRange clear_range = {};
        clear_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        clear_range.layerCount = 1;
        clear_range.levelCount = 1;

        tex.api_ctx()->vkCmdClearDepthStencilImage(cmd_buf, tex.handle().img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   &clear_val, 1, &clear_range);
    }
}

////////////////////////////////////////////////////////////////////////////////////////

Ren::Texture1D::Texture1D(std::string_view name, const BufferRef &buf, const eTexFormat format, const uint32_t offset,
                          const uint32_t size, ILog *log)
    : name_(name) {
    Init(buf, format, offset, size, log);
}

void Ren::Texture1D::Free() {
    if (buf_view_) {
        api_ctx_->buf_views_to_destroy[api_ctx_->backend_frame].push_back(buf_view_);
        buf_view_ = {};
    }
    buf_ = WeakBufferRef{};
}

void Ren::Texture1D::FreeImmediate() {
    if (buf_view_) {
        api_ctx_->vkDestroyBufferView(api_ctx_->device, buf_view_, nullptr);
        buf_view_ = {};
    }
    buf_ = WeakBufferRef{};
}

Ren::Texture1D &Ren::Texture1D::operator=(Texture1D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    api_ctx_ = rhs.buf_->api_ctx();
    buf_ = std::move(rhs.buf_);
    params_ = std::exchange(rhs.params_, {});
    name_ = std::move(rhs.name_);
    buf_view_ = std::exchange(rhs.buf_view_, {});

    return (*this);
}

void Ren::Texture1D::Init(const BufferRef &buf, const eTexFormat format, const uint32_t offset, const uint32_t size,
                          ILog *log) {
    Free();

    VkBufferViewCreateInfo view_info = {VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
    view_info.buffer = buf->vk_handle();
    view_info.format = g_vk_formats[size_t(format)];
    view_info.offset = VkDeviceSize(offset);
    view_info.range = VkDeviceSize(size);

    const VkResult res = buf->api_ctx()->vkCreateBufferView(buf->api_ctx()->device, &view_info, nullptr, &buf_view_);
    assert(res == VK_SUCCESS);

    api_ctx_ = buf->api_ctx();
    buf_ = buf;
    params_.offset = offset;
    params_.size = size;
    params_.format = format;
}

////////////////////////////////////////////////////////////////////////////////////////

Ren::Texture3D::Texture3D(std::string_view name, ApiContext *ctx, const Tex3DParams &params, MemAllocators *mem_allocs,
                          ILog *log)
    : name_(name), api_ctx_(ctx) {
    Init(params, mem_allocs, log);
}

Ren::Texture3D::~Texture3D() { Free(); }

Ren::Texture3D &Ren::Texture3D::operator=(Texture3D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Free();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    alloc_ = std::exchange(rhs.alloc_, {});
    params = std::exchange(rhs.params, {});
    name_ = std::move(rhs.name_);

    resource_state = std::exchange(rhs.resource_state, eResState::Undefined);

    return (*this);
}

void Ren::Texture3D::Init(const Tex3DParams &p, MemAllocators *mem_allocs, ILog *log) {
    Free();

    handle_.generation = TextureHandleCounter++;
    params = p;

    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_3D;
        img_info.extent.width = uint32_t(p.w);
        img_info.extent.height = uint32_t(p.h);
        img_info.extent.depth = uint32_t(p.d);
        img_info.mipLevels = 1;
        img_info.arrayLayers = 1;
        img_info.format = g_vk_formats[size_t(p.format)];
        if (p.flags & eTexFlags::SRGB) {
            img_info.format = ToSRGBFormat(img_info.format);
        }
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        assert(uint8_t(p.usage) != 0);
        img_info.usage = to_vk_image_usage(p.usage, p.format);

        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VK_SAMPLE_COUNT_1_BIT;
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

        res = api_ctx_->vkBindImageMemory(api_ctx_->device, handle_.img, alloc_.owner->mem(alloc_.pool),
                                          VkDeviceSize(alloc_.offset));
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return;
        }
    }

    { // create default image view(s)
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = handle_.img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
        view_info.format = g_vk_formats[size_t(p.format)];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (GetColorChannelCount(p.format) == 1) {
            view_info.components.r = VK_COMPONENT_SWIZZLE_R;
            view_info.components.g = VK_COMPONENT_SWIZZLE_R;
            view_info.components.b = VK_COMPONENT_SWIZZLE_R;
            view_info.components.a = VK_COMPONENT_SWIZZLE_R;
        }

        const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &handle_.views[0]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return;
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

    { // create new sampler
        VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler_info.magFilter = g_vk_min_mag_filter[size_t(p.sampling.filter)];
        sampler_info.minFilter = g_vk_min_mag_filter[size_t(p.sampling.filter)];
        sampler_info.addressModeU = g_vk_wrap_mode[size_t(p.sampling.wrap)];
        sampler_info.addressModeV = g_vk_wrap_mode[size_t(p.sampling.wrap)];
        sampler_info.addressModeW = g_vk_wrap_mode[size_t(p.sampling.wrap)];
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.maxAnisotropy = AnisotropyLevel;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = g_vk_compare_ops[size_t(p.sampling.compare)];
        sampler_info.mipmapMode = g_vk_mipmap_mode[size_t(p.sampling.filter)];
        sampler_info.mipLodBias = p.sampling.lod_bias.to_float();
        sampler_info.minLod = p.sampling.min_lod.to_float();
        sampler_info.maxLod = p.sampling.max_lod.to_float();

        const VkResult res = api_ctx_->vkCreateSampler(api_ctx_->device, &sampler_info, nullptr, &handle_.sampler);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create sampler!");
        }
    }
}

void Ren::Texture3D::Free() {
    if (params.format != eTexFormat::Undefined && !(params.flags & eTexFlags::NoOwnership)) {
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

void Ren::Texture3D::SetSubImage(int offsetx, int offsety, int offsetz, int sizex, int sizey, int sizez,
                                 eTexFormat format, const Buffer &sbuf, CommandBuffer cmd_buf, int data_off,
                                 int data_len) {
    assert(format == params.format);
    assert(offsetx >= 0 && offsetx + sizex <= params.w);
    assert(offsety >= 0 && offsety + sizey <= params.h);
    assert(offsetz >= 0 && offsetz + sizez <= params.d);
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
        new_barrier.subresourceRange.levelCount = 1;
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = 1;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                       0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                       uint32_t(img_barriers.size()), img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy region = {};

    region.bufferOffset = VkDeviceSize(data_off);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {int32_t(offsetx), int32_t(offsety), int32_t(offsetz)};
    region.imageExtent = {uint32_t(sizex), uint32_t(sizey), uint32_t(sizez)};

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                     &region);
}

VkFormat Ren::VKFormatFromTexFormat(eTexFormat format) { return g_vk_formats[size_t(format)]; }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
