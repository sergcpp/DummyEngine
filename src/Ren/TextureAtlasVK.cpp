#include "TextureAtlas.h"

//#include "GL.h"
#include "Utils.h"

namespace Ren {
extern const VkFormat g_vk_formats[];
} // namespace Ren

Ren::TextureAtlas::TextureAtlas(const int w, const int h, const int min_res, const eTexFormat *formats,
                                const uint32_t *flags, eTexFilter filter, ILog *log)
    : splitter_(w, h) {
    filter_ = filter;

    const int mip_count = CalcMipCount(w, h, min_res, filter);

    for (int i = 0; i < MaxTextureCount; i++) {
        if (formats[i] == eTexFormat::Undefined) {
            break;
        }

#if 0
        const GLenum compressed_tex_format =
#if !defined(__ANDROID__)
            (flags[i] & TexSRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                                 : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
            (flags[i] & TexSRGB) ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
                                 : GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif

        formats_[i] = formats[i];

        GLuint tex_id;
        glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);

        GLenum internal_format;

        const int blank_block_res = 64;
        uint8_t blank_block[blank_block_res * blank_block_res * 4] = {};
        if (IsCompressedFormat(formats[i])) {
            for (int j = 0; j < (blank_block_res / 4) * (blank_block_res / 4) * 16;) {
#if defined(__ANDROID__)
                memcpy(&blank_block[j], Ren::_blank_ASTC_block_4x4,
                       Ren::_blank_ASTC_block_4x4_len);
                j += Ren::_blank_ASTC_block_4x4_len;
#else
                memcpy(&blank_block[j], Ren::_blank_DXT5_block_4x4,
                       Ren::_blank_DXT5_block_4x4_len);
                j += Ren::_blank_DXT5_block_4x4_len;
#endif
            }
            internal_format = compressed_tex_format;
        } else {
            internal_format =
                GLInternalFormatFromTexFormat(formats_[i], (flags[i] & TexSRGB) != 0);
        }

        ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count, internal_format, w,
                                    h);

        for (int level = 0; level < mip_count; level++) {
            const int _w = int((unsigned)w >> (unsigned)level),
                      _h = int((unsigned)h >> (unsigned)level),
                      _init_res = std::min(blank_block_res, std::min(_w, _h));
            for (int y_off = 0; y_off < _h; y_off += blank_block_res) {
                const int buf_len =
#if defined(__ANDROID__)
                    // TODO: '+ y_off' fixes an error on Qualcomm (wtf ???)
                    (_init_res / 4) * ((_init_res + y_off) / 4) * 16;
#else
                    (_init_res / 4) * (_init_res / 4) * 16;
#endif

                for (int x_off = 0; x_off < _w; x_off += blank_block_res) {
                    if (IsCompressedFormat(formats[i])) {
                        ren_glCompressedTextureSubImage2D_Comp(
                            GL_TEXTURE_2D, tex_id, level, x_off, y_off, _init_res,
                            _init_res, internal_format, buf_len, blank_block);
                    } else {
                        ren_glTextureSubImage2D_Comp(
                            GL_TEXTURE_2D, tex_id, level, x_off, y_off, _init_res,
                            _init_res, internal_format, GL_UNSIGNED_BYTE, blank_block);
                    }
                }
            }
        }

        const float anisotropy = 4.0f;
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                     anisotropy);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[(size_t)filter_]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[(size_t)filter_]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S,
                                     g_gl_wrap_mode[(size_t)filter]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T,
                                     g_gl_wrap_mode[(size_t)filter]);

        CheckError("create texture", log);

        tex_ids_[i] = (uint32_t)tex_id;
#endif
    }
}

Ren::TextureAtlas::~TextureAtlas() {
    /*for (const uint32_t tex_id : tex_ids_) {
        if (tex_id != 0xffffffff) {
            auto _tex_id = (GLuint)tex_id;
            glDeleteTextures(1, &_tex_id);
        }
    }*/
}

Ren::TextureAtlas::TextureAtlas(TextureAtlas &&rhs) noexcept
    : splitter_(std::move(rhs.splitter_)), filter_(rhs.filter_) {
    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = eTexFormat::Undefined;

        // tex_ids_[i] = rhs.tex_ids_[i];
        // rhs.tex_ids_[i] = 0xffffffff;
    }
}

Ren::TextureAtlas &Ren::TextureAtlas::operator=(TextureAtlas &&rhs) noexcept {
    filter_ = rhs.filter_;

    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = eTexFormat::Undefined;

        /* if (tex_ids_[i] != 0xffffffff) {
             auto tex_id = (GLuint)tex_ids_[i];
             glDeleteTextures(1, &tex_id);
         }
         tex_ids_[i] = rhs.tex_ids_[i];
         rhs.tex_ids_[i] = 0xffffffff;*/
    }

    splitter_ = std::move(rhs.splitter_);
    return (*this);
}

int Ren::TextureAtlas::AllocateRegion(const int res[2], int out_pos[2]) {
    const int index = splitter_.Allocate(res, out_pos);
    return index;
}

void Ren::TextureAtlas::InitRegion(const void *data, const int data_len, const eTexFormat format, const uint32_t flags,
                                   const int layer, const int level, const int pos[2], const int res[2], ILog *log) {
#ifndef NDEBUG
    if (level == 0) {
        int _res[2];
        int rc = splitter_.FindNode(pos, _res);
        assert(rc != -1);
        assert(_res[0] == res[0] && _res[1] == res[1]);
    }
#endif

#if 0
    if (IsCompressedFormat(format)) {
        const GLenum tex_format =
#if !defined(__ANDROID__)
            (flags & TexSRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                              : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
            (flags & TexSRGB) ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
                              : GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif
        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, (GLuint)tex_ids_[layer],
                                               level, pos[0], pos[1], res[0], res[1],
                                               tex_format, data_len, data);
    } else {
        ren_glTextureSubImage2D_Comp(
            GL_TEXTURE_2D, (GLuint)tex_ids_[layer], level, pos[0], pos[1], res[0], res[1],
            GLFormatFromTexFormat(format), GLTypeFromTexFormat(format), data);
    }
    CheckError("init sub image", log);
#endif
}

bool Ren::TextureAtlas::Free(const int pos[2]) {
    // TODO: fill with black in debug
    return splitter_.Free(pos);
}

void Ren::TextureAtlas::Finalize() {
    if (filter_ == eTexFilter::Trilinear || filter_ == eTexFilter::Bilinear) {
        for (int i = 0; i < MaxTextureCount && (formats_[i] != eTexFormat::Undefined); i++) {
            if (!IsCompressedFormat(formats_[i])) {
                // ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_2D, (GLuint)tex_ids_[i]);
            }
        }
    }
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
        vkDestroyImageView(api_ctx_->device, img_view_, nullptr);
        vkDestroyImage(api_ctx_->device, img_, nullptr);
        vkFreeMemory(api_ctx_->device, mem_, nullptr);
    }
}

Ren::TextureAtlasArray &Ren::TextureAtlasArray::operator=(TextureAtlasArray &&rhs) noexcept {
    mip_count_ = exchange(rhs.mip_count_, 0);
    layer_count_ = exchange(rhs.layer_count_, 0);
    format_ = exchange(rhs.format_, eTexFormat::Undefined);
    filter_ = exchange(rhs.filter_, eTexFilter::NoFilter);

    if (img_ != VK_NULL_HANDLE) {
        vkDestroyImageView(api_ctx_->device, img_view_, nullptr);
        vkDestroyImage(api_ctx_->device, img_, nullptr);
        vkFreeMemory(api_ctx_->device, mem_, nullptr);
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

int Ren::TextureAtlasArray::Allocate(const void *data, const eTexFormat format, const int res[2], int out_pos[3],
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
}

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

            sbuf.resource_state = eResState::CopySrc;
            this->resource_state = eResState::CopyDst;

            return index;
        }
    }

    return -1;
}

bool Ren::TextureAtlasArray::Free(const int pos[3]) {
    // TODO: fill with black in debug
    return splitters_[pos[2]].Free(pos);
}
