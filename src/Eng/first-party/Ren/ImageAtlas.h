#pragma once

#include "Image.h"
#include "ImageSplitter.h"
#include "Resource.h"
#include "Sampler.h"
#include "SmallVector.h"

namespace Ren {
class ImageAtlas {
  public:
    static const int MaxImageCount = 8;

    ImageAtlas() : splitter_(0, 0) {}
    ImageAtlas(ApiContext *api_ctx, int w, int h, int min_res, int mip_count, const eFormat formats[],
               const Bitmask<eImgFlags> flags[], eFilter filter, ILog *log);
    ~ImageAtlas();

    ImageAtlas(const ImageAtlas &rhs) = delete;
    ImageAtlas(ImageAtlas &&rhs) noexcept;

    ImageAtlas &operator=(const ImageAtlas &rhs) = delete;
    ImageAtlas &operator=(ImageAtlas &&rhs) noexcept;

    int resx() const { return splitter_.resx(); }
    int resy() const { return splitter_.resy(); }
#if defined(REN_VK_BACKEND)
    VkDescriptorImageInfo vk_desc_image_info(const int view_index = 0,
                                             VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const {
        VkDescriptorImageInfo ret;
        ret.sampler = sampler_.vk_handle();
        ret.imageView = img_view_[view_index];
        ret.imageLayout = layout;
        return ret;
    }
#elif defined(REN_GL_BACKEND)
    uint32_t tex_id(const int i) const { return tex_ids_[i]; }
#endif
    ApiContext *api_ctx() { return api_ctx_; }

    int AllocateRegion(const int res[2], int out_pos[2]);
    void InitRegion(const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf, eFormat format,
                    Bitmask<eImgFlags> flags, int layer, int level, const int pos[2], const int res[2], ILog *log);

    bool Free(const int pos[2]);

    // create mipmaps, compress etc.
    void Finalize(CommandBuffer cmd_buf);

#if defined(REN_VK_BACKEND)
    eResState resource_state = eResState::Undefined;
#endif
  private:
    ApiContext *api_ctx_ = nullptr;
    int mip_count_ = 0;

    eFormat formats_[MaxImageCount] = {eFormat::Undefined, eFormat::Undefined, eFormat::Undefined, eFormat::Undefined,
                                       eFormat::Undefined, eFormat::Undefined, eFormat::Undefined, eFormat::Undefined};
    eFilter filter_ = eFilter::Nearest;
#if defined(REN_VK_BACKEND)
    VkImage img_[MaxImageCount] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                   VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDeviceMemory mem_[MaxImageCount] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                          VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImageView img_view_[MaxImageCount] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    Sampler sampler_;
#elif defined(REN_GL_BACKEND)
    uint32_t tex_ids_[MaxImageCount] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
#endif

    ImageSplitter splitter_;
};

class ImageAtlasArray {
  public:
    ImageAtlasArray() = default;
    ImageAtlasArray(ApiContext *api_ctx, std::string_view name, int w, int h, int layer_count, int mip_count,
                    eFormat format, eFilter filter, const Bitmask<eImgUsage> usage);

    ImageAtlasArray(const ImageAtlasArray &rhs) = delete;
    ImageAtlasArray(ImageAtlasArray &&rhs) noexcept { (*this) = std::move(rhs); }

    ImageAtlasArray &operator=(const ImageAtlasArray &rhs) = delete;
    ImageAtlasArray &operator=(ImageAtlasArray &&rhs) noexcept;

    std::string_view name() const { return name_; }

    int mip_count() const { return mip_count_; }
    int layer_count() const { return layer_count_; }

    int w() const { return w_; }
    int h() const { return h_; }

    eFormat format() const { return format_; }

#if defined(REN_VK_BACKEND)
    VkImage img() const { return img_; }
    VkImageView img_view() const { return img_view_; }
    const Sampler &sampler() const { return sampler_; }
#elif defined(REN_GL_BACKEND)
    uint32_t id() const { return tex_id_; }
#endif
    ApiContext *api_ctx() { return api_ctx_; }

    void Free();
    void FreeImmediate();

    void SetSubImage(int level, int layer, int offsetx, int offsety, int sizex, int sizey, eFormat format,
                     const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf);
    void Clear(const float rgba[4], CommandBuffer cmd_buf);

    int Allocate(const Buffer &sbuf, int data_off, int data_len, CommandBuffer cmd_buf, eFormat format,
                 const int res[2], int out_pos[3], int border);
    bool Free(const int pos[3]);

  private:
    ApiContext *api_ctx_ = nullptr;
    std::string name_;
    int w_ = 0, h_ = 0;
    int mip_count_ = 0;
    int layer_count_ = 0;
    eFormat format_ = eFormat::Undefined;
    eFilter filter_ = eFilter::Nearest;
#if defined(REN_VK_BACKEND)
    VkImage img_ = VK_NULL_HANDLE;
    VkDeviceMemory mem_ = VK_NULL_HANDLE;
    VkImageView img_view_ = VK_NULL_HANDLE;
    Sampler sampler_;
#elif defined(REN_GL_BACKEND)
    uint32_t tex_id_ = 0xffffffff;
#endif

    SmallVector<ImageSplitter, 8> splitters_;

  public:
    mutable eResState resource_state = eResState::Undefined;
};
} // namespace Ren