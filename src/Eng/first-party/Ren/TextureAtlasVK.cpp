#include "TextureAtlas.h"

#include <stdexcept>

//#include "GL.h"
#include "Utils.h"
#include "VKCtx.h"

namespace Ren {
extern const VkFormat g_vk_formats[];

VkFormat ToSRGBFormat(const VkFormat format);
} // namespace Ren

Ren::TextureAtlas::TextureAtlas(ApiContext *api_ctx, const int w, const int h, const int min_res,
                                const eTexFormat formats[], const eTexFlags flags[], eTexFilter filter, ILog *log)
    : api_ctx_(api_ctx), splitter_(w, h) {
    filter_ = filter;

    mip_count_ = CalcMipCount(w, h, min_res, filter);

    for (int i = 0; i < MaxTextureCount; i++) {
        if (formats[i] == eTexFormat::Undefined) {
            break;
        }

        { // create image
            VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            img_info.imageType = VK_IMAGE_TYPE_2D;
            img_info.extent.width = uint32_t(w);
            img_info.extent.height = uint32_t(h);
            img_info.extent.depth = 1;
            img_info.mipLevels = mip_count_;
            img_info.arrayLayers = 1;
            img_info.format = g_vk_formats[size_t(formats[i])];
            if (bool(flags[i] & eTexFlagBits::SRGB)) {
                img_info.format = ToSRGBFormat(img_info.format);
            }
            img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            img_info.usage =
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            img_info.samples = VK_SAMPLE_COUNT_1_BIT;
            img_info.flags = 0;

            VkResult res = vkCreateImage(api_ctx_->device, &img_info, nullptr, &img_[i]);
            if (res != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image!");
            }

            VkMemoryRequirements img_tex_mem_req = {};
            vkGetImageMemoryRequirements(api_ctx_->device, img_[i], &img_tex_mem_req);

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
            res = vkAllocateMemory(api_ctx_->device, &img_alloc_info, nullptr, &mem_[i]);
            if (res != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate memory!");
            }

            res = vkBindImageMemory(api_ctx_->device, img_[i], mem_[i], 0);
            if (res != VK_SUCCESS) {
                throw std::runtime_error("Failed to bind memory!");
            }
        }

        { // create default image view
            VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view_info.image = img_[i];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = g_vk_formats[size_t(formats[i])];
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = mip_count_;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            const VkResult res = vkCreateImageView(api_ctx_->device, &view_info, nullptr, &img_view_[i]);
            if (res != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view!");
            }
        }
    }

    SamplingParams params;
    params.filter = filter;

    sampler_.Init(api_ctx_, params);
}

Ren::TextureAtlas::~TextureAtlas() {
    for (int i = 0; i < MaxTextureCount; ++i) {
        if (img_[i] != VK_NULL_HANDLE) {
            api_ctx_->image_views_to_destroy[api_ctx_->backend_frame].push_back(img_view_[i]);
            api_ctx_->images_to_destroy[api_ctx_->backend_frame].push_back(img_[i]);
            api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(mem_[i]);
        }
    }
}

Ren::TextureAtlas::TextureAtlas(TextureAtlas &&rhs) noexcept
    : api_ctx_(rhs.api_ctx_), splitter_(std::move(rhs.splitter_)), filter_(rhs.filter_) {
    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = eTexFormat::Undefined;

        img_[i] = exchange(rhs.img_[i], VK_NULL_HANDLE);
        img_view_[i] = exchange(rhs.img_view_[i], VK_NULL_HANDLE);
        mem_[i] = exchange(rhs.mem_[i], VK_NULL_HANDLE);

        sampler_ = std::move(rhs.sampler_);
    }
}

Ren::TextureAtlas &Ren::TextureAtlas::operator=(TextureAtlas &&rhs) noexcept {
    api_ctx_ = rhs.api_ctx_;
    filter_ = rhs.filter_;

    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = eTexFormat::Undefined;

        if (img_[i] != VK_NULL_HANDLE) {
            api_ctx_->image_views_to_destroy[api_ctx_->backend_frame].push_back(img_view_[i]);
            api_ctx_->images_to_destroy[api_ctx_->backend_frame].push_back(img_[i]);
            api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(mem_[i]);
        }

        img_[i] = exchange(rhs.img_[i], VK_NULL_HANDLE);
        img_view_[i] = exchange(rhs.img_view_[i], VK_NULL_HANDLE);
        mem_[i] = exchange(rhs.mem_[i], VK_NULL_HANDLE);
    }

    splitter_ = std::move(rhs.splitter_);
    sampler_ = std::move(rhs.sampler_);
    return (*this);
}

int Ren::TextureAtlas::AllocateRegion(const int res[2], int out_pos[2]) {
    const int index = splitter_.Allocate(res, out_pos);
    return index;
}

void Ren::TextureAtlas::InitRegion(const Buffer &sbuf, const int data_off, const int data_len, void *_cmd_buf,
                                   const eTexFormat format, const eTexFlags flags, const int layer, const int level,
                                   const int pos[2], const int res[2], ILog *log) {
#ifndef NDEBUG
    if (level == 0) {
        int _res[2];
        int rc = splitter_.FindNode(pos, _res);
        assert(rc != -1);
        assert(_res[0] == res[0] && _res[1] == res[1]);
    }
#endif

    assert(sbuf.type() == eBufType::Stage);
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

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
        new_barrier.offset = VkDeviceSize(data_off);
        new_barrier.size = VkDeviceSize(data_len);

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
        new_barrier.image = img_[layer];
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS; // transit whole image

        src_stages |= VKPipelineStagesForState(resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, 0, 0,
                             nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                             uint32_t(img_barriers.size()), img_barriers.cdata());
    }

    sbuf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;

    VkBufferImageCopy region = {};
    region.bufferOffset = VkDeviceSize(data_off);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = level;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {int32_t(pos[0]), int32_t(pos[1]), 0};
    region.imageExtent = {uint32_t(res[0]), uint32_t(res[1]), 1};

    vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), img_[layer], VKImageLayoutForState(eResState::CopyDst), 1,
                           &region);
}

bool Ren::TextureAtlas::Free(const int pos[2]) {
    // TODO: fill with black in debug
    return splitter_.Free(pos);
}

void Ren::TextureAtlas::Finalize(void *_cmd_buf) {
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    SmallVector<VkImageMemoryBarrier, MaxTextureCount> img_barriers;
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;

    for (int i = 0; i < MaxTextureCount && (resource_state != eResState::ShaderResource); ++i) {
        if (!img_[i]) {
            continue;
        }

        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::ShaderResource);
        new_barrier.oldLayout = (VkImageLayout)VKImageLayoutForState(resource_state);
        new_barrier.newLayout = (VkImageLayout)VKImageLayoutForState(eResState::ShaderResource);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = img_[i];
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS; // transit whole image

        src_stages |= VKPipelineStagesForState(resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::ShaderResource);
    }

    if (!img_barriers.empty()) {
        vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, 0, 0,
                             nullptr, 0, nullptr, uint32_t(img_barriers.size()), img_barriers.cdata());
    }

    resource_state = eResState::ShaderResource;
}

////////////////////////////////////////////////////////////////////////////////////////////

Ren::TextureAtlasArray::TextureAtlasArray(ApiContext *api_ctx, const int w, const int h, const int layer_count,
                                          const eTexFormat format, eTexFilter filter)
    : layer_count_(layer_count), format_(format), filter_(filter), api_ctx_(api_ctx) {

    mip_count_ = Ren::CalcMipCount(w, h, 1, filter);

    { // create image
        VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.extent.width = uint32_t(w);
        img_info.extent.height = uint32_t(h);
        img_info.extent.depth = 1;
        img_info.mipLevels = mip_count_;
        img_info.arrayLayers = uint32_t(layer_count);
        img_info.format = g_vk_formats[size_t(format)];
        img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        img_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img_info.samples = VK_SAMPLE_COUNT_1_BIT;
        img_info.flags = 0;

        VkResult res = vkCreateImage(api_ctx_->device, &img_info, nullptr, &img_);
        if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }

        VkMemoryRequirements img_tex_mem_req = {};
        vkGetImageMemoryRequirements(api_ctx_->device, img_, &img_tex_mem_req);

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
        res = vkAllocateMemory(api_ctx_->device, &img_alloc_info, nullptr, &mem_);
        if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate memory!");
        }

        res = vkBindImageMemory(api_ctx_->device, img_, mem_, 0);
        if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind memory!");
        }
    }

    { // create default image view
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = img_;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        view_info.format = g_vk_formats[size_t(format)];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = mip_count_;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = layer_count_;

        const VkResult res = vkCreateImageView(api_ctx_->device, &view_info, nullptr, &img_view_);
        if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    SamplingParams params;
    params.filter = filter;

    sampler_.Init(api_ctx_, params);

    for (int i = 0; i < layer_count; i++) {
        splitters_[i] = TextureSplitter{w, h};
    }
}

Ren::TextureAtlasArray::~TextureAtlasArray() {
    if (img_ != VK_NULL_HANDLE) {
        api_ctx_->image_views_to_destroy[api_ctx_->backend_frame].push_back(img_view_);
        api_ctx_->images_to_destroy[api_ctx_->backend_frame].push_back(img_);
        api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(mem_);
    }
}

Ren::TextureAtlasArray &Ren::TextureAtlasArray::operator=(TextureAtlasArray &&rhs) noexcept {
    mip_count_ = exchange(rhs.mip_count_, 0);
    layer_count_ = exchange(rhs.layer_count_, 0);
    format_ = exchange(rhs.format_, eTexFormat::Undefined);
    filter_ = exchange(rhs.filter_, eTexFilter::NoFilter);

    if (img_ != VK_NULL_HANDLE) {
        api_ctx_->image_views_to_destroy[api_ctx_->backend_frame].push_back(img_view_);
        api_ctx_->images_to_destroy[api_ctx_->backend_frame].push_back(img_);
        api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(mem_);
    }

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    img_ = exchange(rhs.img_, {});
    mem_ = exchange(rhs.mem_, {});
    img_view_ = exchange(rhs.img_view_, {});
    sampler_ = exchange(rhs.sampler_, {});

    resource_state = exchange(rhs.resource_state, eResState::Undefined);

    for (int i = 0; i < layer_count_; i++) {
        splitters_[i] = std::move(rhs.splitters_[i]);
    }

    return (*this);
}

/*int Ren::TextureAtlasArray::Allocate(const void *data, const eTexFormat format, const int res[2], int out_pos[3],
                                     const int border) {
    const int alloc_res[] = {res[0] < splitters_[0].resx() ? res[0] + border : res[0],
                             res[1] < splitters_[1].resy() ? res[1] + border : res[1]};

    for (int i = 0; i < layer_count_; i++) {
        const int index = splitters_[i].Allocate(alloc_res, out_pos);
        if (index != -1) {
            out_pos[2] = i;

            // ren_glTextureSubImage3D_Comp(GL_TEXTURE_2D_ARRAY, (GLuint)tex_id_, 0,
            //                             out_pos[0], out_pos[1], out_pos[2], res[0],
            //                             res[1], 1, GLFormatFromTexFormat(format),
            //                             GLTypeFromTexFormat(format), data);
            return index;
        }
    }

    return -1;
}*/

int Ren::TextureAtlasArray::Allocate(const Buffer &sbuf, int data_off, int data_len, void *_cmd_buf, eTexFormat format,
                                     const int res[2], int out_pos[3], int border) {
    const int alloc_res[] = {res[0] < splitters_[0].resx() ? res[0] + border : res[0],
                             res[1] < splitters_[1].resy() ? res[1] + border : res[1]};

    for (int i = 0; i < layer_count_; i++) {
        const int index = splitters_[i].Allocate(alloc_res, out_pos);
        if (index != -1) {
            out_pos[2] = i;

            assert(sbuf.type() == eBufType::Stage);
            VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

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
                new_barrier.offset = VkDeviceSize(data_off);
                new_barrier.size = VkDeviceSize(data_len);

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

            if (!buf_barriers.empty() || !img_barriers.empty()) {
                vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages,
                                     0, 0, nullptr, uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                     uint32_t(img_barriers.size()), img_barriers.cdata());
            }

            sbuf.resource_state = eResState::CopySrc;
            this->resource_state = eResState::CopyDst;

            VkBufferImageCopy region = {};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = uint32_t(i);
            region.imageSubresource.layerCount = 1;

            region.imageOffset = {int32_t(out_pos[0]), int32_t(out_pos[1]), 0};
            region.imageExtent = {uint32_t(res[0]), uint32_t(res[1]), 1};

            vkCmdCopyBufferToImage(cmd_buf, sbuf.vk_handle(), img_, VKImageLayoutForState(eResState::CopyDst), 1,
                                   &region);

            return index;
        }
    }

    return -1;
}

bool Ren::TextureAtlasArray::Free(const int pos[3]) {
    // TODO: fill with black in debug
    return splitters_[pos[2]].Free(pos);
}
