#include "ProbeStorage.h"

#include "Context.h"
#include "Utils.h"
#include "VKCtx.h"

namespace Ren {
uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties *mem_properties, uint32_t mem_type_bits,
                        VkMemoryPropertyFlags desired_mem_flags);
} // namespace Ren

Ren::ProbeStorage::ProbeStorage() = default;

Ren::ProbeStorage::~ProbeStorage() { Destroy(); }

bool Ren::ProbeStorage::Resize(ApiContext *api_ctx, MemoryAllocators *mem_allocs, const eTexFormat format,
                               const int res, const int capacity, ILog *log) {
    const int mip_count = CalcMipCount(res, res, 16, eTexFilter::Bilinear);

    Destroy();

    { // create new image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(res);
        img_info.extent.height = uint32_t(res);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count;
        img_info.arrayLayers = uint32_t(capacity) * 6;
        img_info.format = VKFormatFromTexFormat(format);
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        if (!IsCompressedFormat(format)) {
            img_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VkSampleCountFlagBits(1);
        img_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VkResult res = vkCreateImage(api_ctx->device, &img_info, nullptr, &handle_.img);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image!");
            return false;
        }

#ifdef ENABLE_OBJ_LABELS
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(handle_.img);
        name_info.pObjectName = "Probe Storage";
        vkSetDebugUtilsObjectNameEXT(api_ctx->device, &name_info);
#endif

        VkMemoryRequirements tex_mem_req;
        vkGetImageMemoryRequirements(api_ctx->device, handle_.img, &tex_mem_req);

        alloc_ = mem_allocs->Allocate(
            uint32_t(tex_mem_req.size), uint32_t(tex_mem_req.alignment),
            FindMemoryType(&api_ctx->mem_properties, tex_mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
            "Probe Storage");

        const VkDeviceSize aligned_offset = AlignTo(VkDeviceSize(alloc_.alloc_off), tex_mem_req.alignment);

        res = vkBindImageMemory(api_ctx->device, handle_.img, alloc_.owner->mem(alloc_.block_ndx), aligned_offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return false;
        }
    }

    { // create default image view
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = handle_.img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        view_info.format = VKFormatFromTexFormat(format);
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = capacity * 6;

        const VkResult res = vkCreateImageView(api_ctx->device, &view_info, nullptr, &handle_.views[0]);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create image view!");
            return false;
        }
    }

    this->resource_state = eResState::Undefined;

    const uint32_t BlankBlockRes = 64;

    Buffer temp_stage_buf("Temp probe stage buf", api_ctx, eBufType::Stage, BlankBlockRes * BlankBlockRes * 4);
    uint8_t *blank_block = temp_stage_buf.Map(BufMapWrite);

    if (IsCompressedFormat(format)) {
        for (int i = 0; i < (BlankBlockRes / 4) * (BlankBlockRes / 4) * 16;) {
#if defined(__ANDROID__)
            memcpy(&blank_block[i], _blank_ASTC_block_4x4, _blank_ASTC_block_4x4_len);
            i += _blank_ASTC_block_4x4_len;
#else
            memcpy(&blank_block[i], _blank_DXT5_block_4x4, _blank_DXT5_block_4x4_len);
            i += _blank_DXT5_block_4x4_len;
#endif
        }
    }

    temp_stage_buf.Unmap();

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;
    SmallVector<VkImageMemoryBarrier, 1> img_barriers;

    { // src buffer barrier
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(temp_stage_buf.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = temp_stage_buf.vk_handle();
        new_barrier.offset = VkDeviceSize(0);
        new_barrier.size = VkDeviceSize(temp_stage_buf.size());

        src_stages |= VKPipelineStagesForState(temp_stage_buf.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    { // dst image barrier
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.oldLayout = VKImageLayoutForState(this->resource_state);
        new_barrier.newLayout = VKImageLayoutForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = handle_.img;
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = mip_count; // transit the whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = capacity * 6;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    VkCommandBuffer cmd_buf = BegSingleTimeCommands(api_ctx->device, api_ctx->temp_command_pool);

    vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, 0, 0,
                         nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(), uint32_t(img_barriers.size()),
                         img_barriers.cdata());

    temp_stage_buf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

	std::vector<VkBufferImageCopy> all_regions;

    VkBufferImageCopy proto_region = {};
    proto_region.bufferOffset = VkDeviceSize(0);
    proto_region.bufferRowLength = 0;
    proto_region.bufferImageHeight = 0;

    proto_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    proto_region.imageSubresource.layerCount = 1;

	
    // set texture color to black
    for (int level = 0; level < mip_count; level++) {
        proto_region.imageSubresource.mipLevel = uint32_t(level);

        const uint32_t _res = (unsigned(res) >> unsigned(level)), _init_res = std::min(BlankBlockRes, _res);
        for (int layer = 0; layer < capacity; layer++) {
            for (uint32_t face = 0; face < 6; face++) {
                proto_region.imageSubresource.baseArrayLayer = layer * 6 + face;

                for (uint32_t y_off = 0; y_off < _res; y_off += BlankBlockRes) {
                    const int buf_len =
#if defined(__ANDROID__)
                        // TODO: '+ y_off' fixes an error on Qualcomm (wtf ???)
                        (_init_res / 4) * ((_init_res + y_off) / 4) * 16;
#else
                        (_init_res / 4) * (_init_res / 4) * 16;
#endif

                    for (uint32_t x_off = 0; x_off < _res; x_off += BlankBlockRes) {
                        proto_region.imageOffset = {int32_t(x_off), int32_t(y_off), 0};
                        proto_region.imageExtent = {_init_res, _init_res, 1};

                        all_regions.push_back(proto_region);
                    }
                }
            }
        }
    }

	vkCmdCopyBufferToImage(cmd_buf, temp_stage_buf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           uint32_t(all_regions.size()), all_regions.data());

    EndSingleTimeCommands(api_ctx->device, api_ctx->graphics_queue, cmd_buf, api_ctx->temp_command_pool);

    { // create new sampler
        VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = -16.0f;
        sampler_info.maxLod = +16.0f;

        const VkResult res = vkCreateSampler(api_ctx->device, &sampler_info, nullptr, &handle_.sampler);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create sampler!");
        }
    }

    api_ctx_ = api_ctx;
    format_ = format;
    res_ = res;
    capacity_ = capacity;
    max_level_ = mip_count - 1;

    reserved_temp_layer_ = capacity_ - 1;

    return true;
}

bool Ren::ProbeStorage::SetPixelData(const int level, const int layer, const int face, const eTexFormat format,
                                     const uint8_t *data, const int data_len, ILog *log) {
    if (format_ != format) {
        return false;
    }

    Buffer temp_stage_buf("Temp probe stage buf", api_ctx_, eBufType::Stage, data_len);
    {
        uint8_t *stage_data = temp_stage_buf.Map(BufMapWrite);
        memcpy(stage_data, data, data_len);
        temp_stage_buf.Unmap();
    }

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> buf_barriers;
    SmallVector<VkImageMemoryBarrier, 1> img_barriers;

    { // src buffer barrier
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(temp_stage_buf.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = temp_stage_buf.vk_handle();
        new_barrier.offset = VkDeviceSize(0);
        new_barrier.size = VkDeviceSize(temp_stage_buf.size());

        src_stages |= VKPipelineStagesForState(temp_stage_buf.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    // dst image barrier
    if (this->resource_state != eResState::CopyDst) {
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.oldLayout = VKImageLayoutForState(this->resource_state);
        new_barrier.newLayout = VKImageLayoutForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = handle_.img;
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = (max_level_ + 1); // transit the whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = capacity_ * 6;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    VkCommandBuffer cmd_buf = BegSingleTimeCommands(api_ctx_->device, api_ctx_->temp_command_pool);

    vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, 0, 0,
                         nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(), uint32_t(img_barriers.size()),
                         img_barriers.cdata());

    temp_stage_buf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy region = {};
    region.bufferOffset = VkDeviceSize(0);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = uint32_t(level);
    region.imageSubresource.baseArrayLayer = layer * 6 + face;

    const uint32_t _res = unsigned(res_) >> unsigned(level);
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {_res, _res, 1};

    vkCmdCopyBufferToImage(cmd_buf, temp_stage_buf.vk_handle(), handle_.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    EndSingleTimeCommands(api_ctx_->device, api_ctx_->graphics_queue, cmd_buf, api_ctx_->temp_command_pool);

    return true;
}

bool Ren::ProbeStorage::GetPixelData(const int level, const int layer, const int face, const int buf_size,
                                     uint8_t *out_pixels, ILog *log) const {
#if !defined(__ANDROID__) && 0
    const int mip_res = int(unsigned(res_) >> unsigned(level));
    if (buf_size < 4 * mip_res * mip_res) {
        return false;
    }

    glGetTextureSubImage(GLuint(handle_.id), level, 0, 0, (layer * 6 + face), mip_res, mip_res, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, buf_size, out_pixels);
    CheckError("glGetTextureSubImage", log);

    return true;
#else
    return false;
#endif
}

void Ren::ProbeStorage::Destroy() {
    if (format_ != eTexFormat::Undefined) {
        assert(IsMainThread());

        for (VkImageView view : handle_.views) {
            if (view) {
                api_ctx_->image_views_to_destroy[api_ctx_->backend_frame].push_back(view);
            }
        }
        api_ctx_->images_to_destroy[api_ctx_->backend_frame].push_back(handle_.img);
        api_ctx_->samplers_to_destroy[api_ctx_->backend_frame].push_back(handle_.sampler);
        api_ctx_->allocs_to_free[api_ctx_->backend_frame].emplace_back(std::move(alloc_));

        handle_ = {};
        format_ = eTexFormat::Undefined;
    }
}

void Ren::ProbeStorage::Finalize() {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkImageMemoryBarrier, 1> img_barriers;

    // dst image barrier
    if (this->resource_state != eResState::ShaderResource) {
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::ShaderResource);
        new_barrier.oldLayout = VKImageLayoutForState(this->resource_state);
        new_barrier.newLayout = VKImageLayoutForState(eResState::ShaderResource);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = handle_.img;
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = (max_level_ + 1); // transit the whole image
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = capacity_ * 6;

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::ShaderResource);
    }

    if (!img_barriers.empty()) {
        VkCommandBuffer cmd_buf = BegSingleTimeCommands(api_ctx_->device, api_ctx_->temp_command_pool);

        vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, 0, 0,
                             nullptr, 0, nullptr, uint32_t(img_barriers.size()), img_barriers.cdata());
        this->resource_state = eResState::ShaderResource;

        EndSingleTimeCommands(api_ctx_->device, api_ctx_->graphics_queue, cmd_buf, api_ctx_->temp_command_pool);
    }
}