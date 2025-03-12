#include "TextureArray.h"

#include <stdexcept>

#include "Config.h"
#include "Utils.h"
#include "VKCtx.h"

namespace Ren {
extern const VkFormat g_formats_vk[];

VkFormat ToSRGBFormat(VkFormat format);
VkImageUsageFlags to_vk_image_usage(Bitmask<eTexUsage> usage, eTexFormat format);
} // namespace Ren

Ren::Texture2DArray::Texture2DArray(ApiContext *api_ctx, const std::string_view name, const int w, const int h,
                                    const int layer_count, const int mip_count, const eTexFormat format,
                                    const eTexFilter filter, const Bitmask<eTexUsage> usage)
    : api_ctx_(api_ctx), name_(name), w_(w), h_(h), mip_count_(mip_count), layer_count_(layer_count), format_(format),
      filter_(filter) {
    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(w);
        img_info.extent.height = uint32_t(h);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count_;
        img_info.arrayLayers = uint32_t(layer_count);
        img_info.format = g_formats_vk[size_t(format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.usage = to_vk_image_usage(usage, format);
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VK_SAMPLE_COUNT_1_BIT;
        img_info.flags = 0;

        VkResult res = api_ctx_->vkCreateImage(api_ctx_->device, &img_info, nullptr, &img_);
        if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(img_);
        name_info.pObjectName = name_.c_str();
        api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

        VkMemoryRequirements img_tex_mem_req = {};
        api_ctx_->vkGetImageMemoryRequirements(api_ctx_->device, img_, &img_tex_mem_req);

        VkMemoryAllocateInfo img_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        img_alloc_info.allocationSize = img_tex_mem_req.size;

        uint32_t img_tex_type_bits = img_tex_mem_req.memoryTypeBits;
        const VkMemoryPropertyFlags img_tex_desired_mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        for (uint32_t i = 0; i < 32; i++) {
            VkMemoryType mem_type = api_ctx_->mem_properties.memoryTypes[i];
            if (img_tex_type_bits & 1u) {
                if ((mem_type.propertyFlags & img_tex_desired_mem_flags) == img_tex_desired_mem_flags) {
                    img_alloc_info.memoryTypeIndex = i;
                    break;
                }
            }
            img_tex_type_bits = img_tex_type_bits >> 1u;
        }

        // TODO: avoid dedicated allocation
        res = api_ctx_->vkAllocateMemory(api_ctx_->device, &img_alloc_info, nullptr, &mem_);
        if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate memory!");
        }

        res = api_ctx_->vkBindImageMemory(api_ctx_->device, img_, mem_, 0);
        if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind memory!");
        }
    }

    { // create default image view
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = img_;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        view_info.format = g_formats_vk[size_t(format)];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count_;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = layer_count_;

        const VkResult res = api_ctx_->vkCreateImageView(api_ctx_->device, &view_info, nullptr, &img_view_);
        if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    SamplingParams params;
    params.filter = filter;

    sampler_.Init(api_ctx_, params);
}

void Ren::Texture2DArray::Free() {
    if (img_ != VK_NULL_HANDLE) {
        api_ctx_->image_views_to_destroy[api_ctx_->backend_frame].push_back(img_view_);
        api_ctx_->images_to_destroy[api_ctx_->backend_frame].push_back(img_);
        api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(mem_);
        img_ = VK_NULL_HANDLE;
    }
}

void Ren::Texture2DArray::FreeImmediate() {
    if (img_ != VK_NULL_HANDLE) {
        api_ctx_->vkDestroyImageView(api_ctx_->device, img_view_, nullptr);
        api_ctx_->vkDestroyImage(api_ctx_->device, img_, nullptr);
        api_ctx_->vkFreeMemory(api_ctx_->device, mem_, nullptr);
        img_ = VK_NULL_HANDLE;
    }
}

Ren::Texture2DArray &Ren::Texture2DArray::operator=(Texture2DArray &&rhs) noexcept {
    mip_count_ = std::exchange(rhs.mip_count_, 0);
    layer_count_ = std::exchange(rhs.layer_count_, 0);
    format_ = std::exchange(rhs.format_, eTexFormat::Undefined);
    filter_ = std::exchange(rhs.filter_, eTexFilter::Nearest);

    Free();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    img_ = std::exchange(rhs.img_, {});
    mem_ = std::exchange(rhs.mem_, {});
    img_view_ = std::exchange(rhs.img_view_, {});
    sampler_ = std::exchange(rhs.sampler_, {});

    resource_state = std::exchange(rhs.resource_state, eResState::Undefined);

    return (*this);
}

void Ren::Texture2DArray::SetSubImage(const int level, const int layer, const int offsetx, const int offsety,
                                      const int sizex, const int sizey, const eTexFormat format, const Buffer &sbuf,
                                      const int data_off, const int data_len, CommandBuffer cmd_buf) {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;

    if (sbuf.resource_state != eResState::Undefined && sbuf.resource_state != eResState::CopySrc) {
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(sbuf.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = sbuf.vk_handle();
        new_barrier.offset = 0;           // VkDeviceSize(data_off);
        new_barrier.size = VK_WHOLE_SIZE; // VkDeviceSize(data_len);

        src_stages |= VKPipelineStagesForState(sbuf.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    SmallVector<VkImageMemoryBarrier, 1> img_barriers;
    if (resource_state != eResState::CopyDst) {
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.oldLayout = (VkImageLayout)VKImageLayoutForState(resource_state);
        new_barrier.newLayout = (VkImageLayout)VKImageLayoutForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = img_;
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = 1;
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = uint32_t(layer_count_); // transit the whole image

        src_stages |= VKPipelineStagesForState(resource_state);
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
    region.imageSubresource.baseArrayLayer = uint32_t(layer);
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {int32_t(offsetx), int32_t(offsety), 0};
    region.imageExtent = {uint32_t(sizex), uint32_t(sizey), 1};

    api_ctx_->vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), img_,
                                     VkImageLayout(VKImageLayoutForState(eResState::CopyDst)), 1, &region);
}

void Ren::Texture2DArray::Clear(const float rgba[4], CommandBuffer cmd_buf) {
    assert(resource_state == eResState::CopyDst);

    VkClearColorValue clear_val = {};
    memcpy(clear_val.float32, rgba, 4 * sizeof(float));

    VkImageSubresourceRange clear_range = {};
    clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.layerCount = layer_count_;
    clear_range.levelCount = mip_count_;

    api_ctx_->vkCmdClearColorImage(cmd_buf, img_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val, 1, &clear_range);
}