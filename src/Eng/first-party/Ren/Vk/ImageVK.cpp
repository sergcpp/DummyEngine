#include "ImageVK.h"

#include <memory>

#include "../Config.h"
#include "../ImageParams.h"
#include "../Log.h"
#include "../utils/Utils.h"
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
#include "../Format.inl"
};
#undef X

// make sure we can simply cast these
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT == 1);
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT == 2);
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT == 4);
static_assert(VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT == 8);

VkImageUsageFlags to_vk_image_usage(const Bitmask<eImgUsage> usage, const eFormat format) {
    VkImageUsageFlags ret = 0;
    if (usage & eImgUsage::Transfer) {
        ret |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }
    if (usage & eImgUsage::Sampled) {
        ret |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (usage & eImgUsage::Storage) {
        assert(!IsCompressedFormat(format));
        ret |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (usage & eImgUsage::RenderTarget) {
        assert(!IsCompressedFormat(format));
        if (IsDepthFormat(format)) {
            ret |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            ret |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }
    return ret;
}

eFormat FormatFromGLInternalFormat(uint32_t gl_internal_format, bool *is_srgb);
int GetBlockLenBytes(eFormat format);
int GetDataLenBytes(int w, int h, int d, eFormat format);
void ParseDDSHeader(const DDSHeader &hdr, ImgParams *params);

extern const VkFilter g_min_mag_filter_vk[];
extern const VkSamplerAddressMode g_wrap_mode_vk[];
extern const VkSamplerMipmapMode g_mipmap_mode_vk[];
extern const VkCompareOp g_compare_ops_vk[];

extern const float AnisotropyLevel;
} // namespace Ren

bool Ren::Image_Init(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, String name, const ImgParams &p,
                     const BufferMain *sbuf_main, const BufferCold *sbuf_cold, uint32_t data_off, CommandBuffer cmd_buf,
                     MemAllocators *mem_allocs, ILog *log) {
    img_main.resource_state = eResState::Undefined;
    img_cold.name = std::move(name);
    img_cold.params = p;

    if (!img_cold.params.mip_count) {
        img_cold.params.mip_count = CalcMipCount(p.w, p.h, 1);
    }

    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType =
            (p.flags & eImgFlags::Array) ? VK_IMAGE_TYPE_2D : (p.d ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D);
        img_info.extent.width = p.w;
        img_info.extent.height = p.h;
        img_info.extent.depth = (p.flags & eImgFlags::Array) ? 1 : std::max<uint32_t>(p.d, 1u);
        img_info.mipLevels = img_cold.params.mip_count;
        if (p.flags & eImgFlags::CubeMap) {
            img_info.arrayLayers = 6;
        } else {
            img_info.arrayLayers = (p.flags & eImgFlags::Array) ? std::max<uint32_t>(p.d, 1u) : 1;
        }
        img_info.format = g_formats_vk[size_t(p.format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        assert(uint8_t(p.usage) != 0);
        img_info.usage = to_vk_image_usage(p.usage, p.format);

        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VkSampleCountFlagBits(p.samples);
        img_info.flags = (p.flags & eImgFlags::CubeMap) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

        VkResult res = api.vkCreateImage(api.device, &img_info, nullptr, &img_main.img);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return false;
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(img_main.img);
        name_info.pObjectName = img_cold.name.c_str();
        api.vkSetDebugUtilsObjectNameEXT(api.device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        api.vkGetImageMemoryRequirements(api.device, img_main.img, &tex_mem_req);

        VkMemoryPropertyFlags img_tex_desired_mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        img_cold.alloc = mem_allocs->Allocate(tex_mem_req, img_tex_desired_mem_flags);
        if (!img_cold.alloc) {
            log->Warning("Not enough device memory, falling back to CPU RAM!");
            img_tex_desired_mem_flags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            img_cold.alloc = mem_allocs->Allocate(tex_mem_req, img_tex_desired_mem_flags);
        }

        if (!img_cold.alloc) {
            log->Error("Failed to allocate image memory!");
            api.vkDestroyImage(api.device, img_main.img, nullptr);
            img_main.img = {};
            return false;
        }

        res = api.vkBindImageMemory(api.device, img_main.img, img_cold.alloc.owner->mem(img_cold.alloc.pool),
                                    img_cold.alloc.offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            api.vkDestroyImage(api.device, img_main.img, nullptr);
            img_main.img = {};
            return false;
        }
    }

    if (!Image_AddDefaultViews(api, img_main, img_cold, log)) {
        api.vkDestroyImage(api.device, img_main.img, nullptr);
        img_main.img = {};
        return false;
    }

    if (sbuf_main) {
        assert(p.samples == 1);
        assert(sbuf_cold && sbuf_cold->type == eBufType::Upload);

        VkPipelineStageFlags src_stages = 0, dst_stages = 0;
        SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;
        SmallVector<VkImageMemoryBarrier, 1> img_barriers;

        if (sbuf_main->resource_state != eResState::Undefined && sbuf_main->resource_state != eResState::CopySrc) {
            auto &new_barrier = buf_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            new_barrier.srcAccessMask = VKAccessFlagsForState(sbuf_main->resource_state);
            new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.buffer = sbuf_main->buf;
            new_barrier.offset = VkDeviceSize(data_off);
            new_barrier.size = VK_WHOLE_SIZE;

            src_stages |= VKPipelineStagesForState(sbuf_main->resource_state);
            dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
        }

        if (img_main.resource_state != eResState::CopyDst) {
            auto &new_barrier = img_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            new_barrier.srcAccessMask = VKAccessFlagsForState(img_main.resource_state);
            new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
            new_barrier.oldLayout = VkImageLayout(VKImageLayoutForState(img_main.resource_state));
            new_barrier.newLayout = VkImageLayout(VKImageLayoutForState(eResState::CopyDst));
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.image = img_main.img;
            new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            new_barrier.subresourceRange.baseMipLevel = 0;
            new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            new_barrier.subresourceRange.baseArrayLayer = 0;
            new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            src_stages |= VKPipelineStagesForState(img_main.resource_state);
            dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
        }

        src_stages &= api.supported_stages_mask;
        dst_stages &= api.supported_stages_mask;

        if (!buf_barriers.empty() || !img_barriers.empty()) {
            api.vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                     0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                     uint32_t(img_barriers.size()), img_barriers.cdata());
        }

        sbuf_main->resource_state = eResState::CopySrc;
        img_main.resource_state = eResState::CopyDst;

        int bytes_left = sbuf_cold->size - data_off;

        if (p.flags & eImgFlags::CubeMap) {
            SmallVector<VkBufferImageCopy, 16> regions;
            for (int i = 0; i < 6; i++) {
                int w = p.w, h = p.h;
                for (int j = 0; j < img_cold.params.mip_count; ++j) {
                    const int len = GetDataLenBytes(w, h, 1, p.format);
                    if (len > bytes_left) {
                        log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
                        return false;
                    }

                    VkBufferImageCopy &reg = regions.emplace_back();

                    reg.bufferOffset = data_off;
                    reg.bufferRowLength = 0;
                    reg.bufferImageHeight = 0;

                    reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    reg.imageSubresource.mipLevel = j;
                    reg.imageSubresource.baseArrayLayer = i;
                    reg.imageSubresource.layerCount = 1;

                    reg.imageOffset = {0, 0, 0};
                    reg.imageExtent = {uint32_t(w), uint32_t(h), 1};

                    data_off += len;
                    bytes_left -= len;

                    w = std::max(w / 2, 1);
                    h = std::max(h / 2, 1);
                }
            }

            api.vkCmdCopyBufferToImage(cmd_buf, sbuf_main->buf, img_main.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       uint32_t(regions.size()), regions.data());
        } else {
            int w = p.w, h = p.h, d = p.d;
            for (int i = 0; i < img_cold.params.mip_count; ++i) {
                const int len = GetDataLenBytes(w, h, d, p.format);
                if (len > bytes_left) {
                    log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
                    return false;
                }

                VkBufferImageCopy region = {};
                region.bufferOffset = VkDeviceSize(data_off);
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;

                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = i;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = (p.flags & eImgFlags::Array) ? d : 1;

                region.imageOffset = {0, 0, 0};
                region.imageExtent = {uint32_t(w), uint32_t(h),
                                      uint32_t((p.flags & eImgFlags::Array) ? 1 : std::max(d, 1))};

                api.vkCmdCopyBufferToImage(cmd_buf, sbuf_main->buf, img_main.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           1, &region);

                data_off += len;
                bytes_left -= len;
                w = std::max(w / 2, 1);
                h = std::max(h / 2, 1);
                d = std::max(d / 2, 1);
            }
        }
    }

    return Image_SetSampling(api, img_main, img_cold, p.sampling, log);
}

bool Ren::Image_Init(const ApiContext &api, ImageCold &img_cold, String name, const ImgParams &p, MemAllocation &&alloc,
                     ILog *log) {
    img_cold.name = std::move(name);
    img_cold.alloc = std::move(alloc);
    img_cold.params = p;

    return true;
}

void Ren::Image_Destroy(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold) {
    if (!(Bitmask<eImgFlags>{img_cold.params.flags} & eImgFlags::NoOwnership)) {
        for (const VkImageView view : img_main.views) {
            if (view) {
                api.image_views_to_destroy[api.backend_frame].push_back(view);
            }
        }
        if (img_main.img) {
            api.images_to_destroy[api.backend_frame].push_back(img_main.img);
        }
        if (img_main.sampler) {
            api.samplers_to_destroy[api.backend_frame].push_back(img_main.sampler);
        }
        if (img_cold.alloc) {
            api.allocations_to_free[api.backend_frame].emplace_back(std::move(img_cold.alloc));
        }
    }
    img_main = {};
    img_cold = {};
}

void Ren::Image_DestroyImmediately(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold) {
    if (!(Bitmask<eImgFlags>{img_cold.params.flags} & eImgFlags::NoOwnership)) {
        for (const VkImageView view : img_main.views) {
            if (view) {
                api.vkDestroyImageView(api.device, view, nullptr);
            }
        }
        if (img_main.img) {
            api.vkDestroyImage(api.device, img_main.img, nullptr);
        }
        if (img_main.sampler) {
            api.vkDestroySampler(api.device, img_main.sampler, nullptr);
        }
    }
    img_main = {};
    img_cold = {};
}

bool Ren::Image_SetSampling(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, const SamplingParams s,
                            ILog *log) {
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
    sampler_info.compareEnable = s.compare != eCompareOp::None ? VK_TRUE : VK_FALSE;
    sampler_info.compareOp = g_compare_ops_vk[size_t(s.compare)];
    sampler_info.mipmapMode = g_mipmap_mode_vk[size_t(s.filter)];
    sampler_info.mipLodBias = s.lod_bias.to_float();
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;

    VkSampler new_sampler = {};
    const VkResult res = api.vkCreateSampler(api.device, &sampler_info, nullptr, &new_sampler);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create sampler!");
        return false;
    }

    if (img_main.sampler) {
        api.samplers_to_destroy[api.backend_frame].emplace_back(img_main.sampler);
    }

    img_main.sampler = new_sampler;
    img_cold.params.sampling = s;

    return true;
}

void Ren::Image_SetSubImage(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, const int layer,
                            const int level, const Vec3i &offset, const Vec3i &size, const eFormat format,
                            const BufferMain &sbuf_main, CommandBuffer cmd_buf, const int data_off,
                            const int data_len) {
    assert(format == img_cold.params.format);
    assert(img_cold.params.samples == 1);
    assert(offset[0] >= 0 && offset[0] + size[0] <= std::max(img_cold.params.w >> level, 1));
    assert(offset[1] >= 0 && offset[1] + size[1] <= std::max(img_cold.params.h >> level, 1));
    assert(offset[2] >= 0 && offset[2] + size[2] <= std::max(img_cold.params.d >> level, 1));

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;
    SmallVector<VkImageMemoryBarrier, 1> img_barriers;

    if (sbuf_main.resource_state != eResState::Undefined && sbuf_main.resource_state != eResState::CopySrc) {
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(sbuf_main.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = sbuf_main.buf;
        new_barrier.offset = VkDeviceSize(0);
        new_barrier.size = VK_WHOLE_SIZE;

        src_stages |= VKPipelineStagesForState(sbuf_main.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    if (img_main.resource_state != eResState::CopyDst) {
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(img_main.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.oldLayout = VkImageLayout(VKImageLayoutForState(img_main.resource_state));
        new_barrier.newLayout = VkImageLayout(VKImageLayoutForState(eResState::CopyDst));
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = img_main.img;
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        src_stages |= VKPipelineStagesForState(img_main.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api.supported_stages_mask;
    dst_stages &= api.supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        api.vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, 0, 0,
                                 nullptr, buf_barriers.size(), buf_barriers.cdata(), img_barriers.size(),
                                 img_barriers.cdata());
    }

    sbuf_main.resource_state = eResState::CopySrc;
    img_main.resource_state = eResState::CopyDst;

    VkBufferImageCopy region = {};

    region.bufferOffset = VkDeviceSize(data_off);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = uint32_t(level);
    region.imageSubresource.baseArrayLayer = uint32_t(layer);
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {offset[0], offset[1], offset[2]};
    region.imageExtent = {uint32_t(size[0]), uint32_t(size[1]), uint32_t(size[2])};

    api.vkCmdCopyBufferToImage(cmd_buf, sbuf_main.buf, img_main.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

bool Ren::Image_AddDefaultViews(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, ILog *log) {
    const Ren::ImgParams &p = img_cold.params;

    if (p.usage != Bitmask(eImgUsage::Transfer)) {
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = img_main.img;
        if (p.flags & eImgFlags::CubeMap) {
            view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        } else {
            view_info.viewType = (p.flags & eImgFlags::Array) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                                              : (p.d ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D);
        }
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

        VkImageView main_view;
        const VkResult res = api.vkCreateImageView(api.device, &view_info, nullptr, &main_view);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return false;
        }
        img_main.views.push_back(main_view);

        if (IsDepthStencilFormat(p.format)) {
            // create additional depth-only image view
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            VkImageView depth_only_view;
            const VkResult _res = api.vkCreateImageView(api.device, &view_info, nullptr, &depth_only_view);
            if (_res != VK_SUCCESS) {
                log->Error("Failed to create image view!");
                return false;
            }
            img_main.views.push_back(depth_only_view);
        }

#ifdef ENABLE_GPU_DEBUG
        for (const VkImageView view : img_main.views) {
            VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
            name_info.objectHandle = uint64_t(view);
            name_info.pObjectName = img_cold.name.c_str();
            api.vkSetDebugUtilsObjectNameEXT(api.device, &name_info);
        }
#endif
    }

    return true;
}

int Ren::Image_AddView(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, const eFormat format,
                       const int mip_level, const int mip_count, const int base_layer, const int layer_count) {
    const ImgParams &p = img_cold.params;

    VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image = img_main.img;
    view_info.viewType = (p.flags & eImgFlags::Array) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                                      : (p.d ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D);
    view_info.format = g_formats_vk[size_t(format)];
    if (IsDepthStencilFormat(p.format)) {
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    } else if (IsDepthFormat(p.format)) {
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    view_info.subresourceRange.baseMipLevel = mip_level;
    view_info.subresourceRange.levelCount = mip_count;
    view_info.subresourceRange.baseArrayLayer = base_layer;
    view_info.subresourceRange.layerCount = layer_count;

    img_main.views.emplace_back(VK_NULL_HANDLE);
    const VkResult res = api.vkCreateImageView(api.device, &view_info, nullptr, &img_main.views.back());
    if (res != VK_SUCCESS) {
        return -1;
    }

#ifdef ENABLE_GPU_DEBUG
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    name_info.objectHandle = uint64_t(img_main.views.back());
    name_info.pObjectName = img_cold.name.c_str();
    api.vkSetDebugUtilsObjectNameEXT(api.device, &name_info);
#endif

    return int(img_main.views.size()) - 1;
}

void Ren::Image_CmdClear(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, const ClearColor &col,
                         CommandBuffer cmd_buf) {
    assert(img_main.resource_state == eResState::CopyDst);

    VkImageSubresourceRange clear_range = {};
    clear_range.baseMipLevel = 0;
    clear_range.levelCount = VK_REMAINING_MIP_LEVELS;
    clear_range.baseArrayLayer = 0;
    clear_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    if (!IsDepthFormat(img_cold.params.format)) {
        clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkClearColorValue clear_val = {};
        memcpy(clear_val.uint32, col.uint32, 4 * sizeof(float));

        api.vkCmdClearColorImage(cmd_buf, img_main.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val, 1,
                                 &clear_range);
    } else {
        clear_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (IsDepthStencilFormat(img_cold.params.format)) {
            clear_range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkClearDepthStencilValue clear_val = {};
        clear_val.depth = col.float32[0];

        api.vkCmdClearDepthStencilImage(cmd_buf, img_main.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val, 1,
                                        &clear_range);
    }
}

void Ren::Image_CmdCopyToBuffer(const ApiContext &api, const ImageMain &img_main, const ImageCold &img_cold,
                                BufferMain &buf_main, BufferCold &buf_cold, CommandBuffer cmd_buf,
                                const uint32_t data_off) {
    assert(img_main.resource_state == eResState::CopySrc);
    assert(buf_main.resource_state == eResState::CopyDst);

    VkBufferImageCopy region = {};

    region.bufferOffset = VkDeviceSize(data_off);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {uint32_t(img_cold.params.w), uint32_t(img_cold.params.h), 1};

    api.vkCmdCopyImageToBuffer(cmd_buf, img_main.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf_main.buf, 1, &region);
}

void Ren::Image_CmdCopyToImage(const ApiContext &api, CommandBuffer cmd_buf, const ImageMain &src_main,
                               const ImageCold &src_cold, const uint32_t src_level, const Vec3i &src_offset,
                               const ImageMain &dst_main, const ImageCold &dst_cold, const uint32_t dst_level,
                               const Vec3i &dst_offset, const uint32_t dst_face, const Vec3i &size) {
    assert(src_main.resource_state == eResState::CopySrc);
    assert(dst_main.resource_state == eResState::CopyDst);

    VkImageCopy reg;
    if (IsDepthFormat(src_cold.params.format)) {
        reg.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        reg.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    reg.srcSubresource.baseArrayLayer = 0;
    reg.srcSubresource.layerCount = 1;
    reg.srcSubresource.mipLevel = src_level;
    reg.srcOffset = {src_offset[0], src_offset[1], src_offset[2]};
    if (IsDepthFormat(dst_cold.params.format)) {
        reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        reg.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    reg.dstSubresource.baseArrayLayer = dst_face;
    reg.dstSubresource.layerCount = 1;
    reg.dstSubresource.mipLevel = dst_level;
    reg.dstOffset = {dst_offset[0], dst_offset[1], dst_offset[2]};
    reg.extent = {uint32_t(size[0]), uint32_t(size[1]), uint32_t(size[2])};

    api.vkCmdCopyImage(cmd_buf, src_main.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_main.img,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
}

VkDescriptorImageInfo Ren::Image_GetDescriptorImageInfo(const ApiContext &api, const ImageMain &img_main,
                                                        const int view_index, const VkImageLayout layout) {
    VkDescriptorImageInfo ret;
    ret.sampler = img_main.sampler;
    ret.imageView = img_main.views[view_index];
    ret.imageLayout = layout;
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////

VkFormat Ren::VKFormatFromFormat(eFormat format) { return g_formats_vk[size_t(format)]; }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
