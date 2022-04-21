#pragma once

#include "Resource.h"
#include "Sampler.h"
#include "Texture.h"
#include "TextureSplitter.h"

namespace Ren {
class TextureAtlas {
  public:
    static const int MaxTextureCount = 8;

    TextureAtlas() : splitter_(0, 0) {}
    TextureAtlas(ApiContext *api_ctx, int w, int h, int min_res, const eTexFormat formats[], const eTexFlags flags[],
                 eTexFilter filter, ILog *log);
    ~TextureAtlas();

    TextureAtlas(const TextureAtlas &rhs) = delete;
    TextureAtlas(TextureAtlas &&rhs) noexcept;

    TextureAtlas &operator=(const TextureAtlas &rhs) = delete;
    TextureAtlas &operator=(TextureAtlas &&rhs) noexcept;

    int resx() const { return splitter_.resx(); }
    int resy() const { return splitter_.resy(); }
#if defined(USE_VK_RENDER)
    VkDescriptorImageInfo vk_desc_image_info(const int view_index = 0,
                                             VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const {
        VkDescriptorImageInfo ret;
        ret.sampler = sampler_.vk_handle();
        ret.imageView = img_view_[view_index];
        ret.imageLayout = layout;
        return ret;
    }
#elif defined(USE_GL_RENDER)
    uint32_t tex_id(const int i) const { return tex_ids_[i]; }
#endif

    int AllocateRegion(const int res[2], int out_pos[2]);
    void InitRegion(const Buffer &sbuf, int data_off, int data_len, void *cmd_buf, eTexFormat format, eTexFlags flags,
                    int layer, int level, const int pos[2], const int res[2], ILog *log);

    bool Free(const int pos[2]);

    // create mipmaps, compress etc.
    void Finalize(void *cmd_buf);

#if defined(USE_VK_RENDER)
    eResState resource_state = eResState::Undefined;
#endif
  private:
    ApiContext *api_ctx_ = nullptr;
    int mip_count_ = 0;

    eTexFormat formats_[MaxTextureCount] = {eTexFormat::Undefined, eTexFormat::Undefined, eTexFormat::Undefined,
                                            eTexFormat::Undefined, eTexFormat::Undefined, eTexFormat::Undefined,
                                            eTexFormat::Undefined, eTexFormat::Undefined};
    eTexFilter filter_ = eTexFilter::NoFilter;
#if defined(USE_VK_RENDER)
    VkImage img_[MaxTextureCount] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                     VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDeviceMemory mem_[MaxTextureCount] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                            VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImageView img_view_[MaxTextureCount] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                              VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    Sampler sampler_;
#elif defined(USE_GL_RENDER)
    uint32_t tex_ids_[MaxTextureCount] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                          0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
#endif

    TextureSplitter splitter_;
};

class TextureAtlasArray {
  public:
    static const int MaxTextureCount = 8;

    TextureAtlasArray() = default;
    TextureAtlasArray(ApiContext *api_ctx, int w, int h, int layer_count, eTexFormat format, eTexFilter filter);
    ~TextureAtlasArray();

    TextureAtlasArray(const TextureAtlasArray &rhs) = delete;
    TextureAtlasArray(TextureAtlasArray &&rhs) noexcept { (*this) = std::move(rhs); }

    TextureAtlasArray &operator=(const TextureAtlasArray &rhs) = delete;
    TextureAtlasArray &operator=(TextureAtlasArray &&rhs) noexcept;

    int mip_count() const { return mip_count_; }
    int layer_count() const { return layer_count_; }

    int resx() const { return splitters_[0].resx(); }
    int resy() const { return splitters_[0].resy(); }
#if defined(USE_VK_RENDER)
    VkImage img() const { return img_; }
    VkImageView img_view() const { return img_view_; }
    const Sampler &sampler() const { return sampler_; }
#elif defined(USE_GL_RENDER)
    uint32_t tex_id() const { return tex_id_; }
#endif

    // int Allocate(const void *data, eTexFormat format, const int res[2], int out_pos[3], int border);
    int Allocate(const Buffer &sbuf, int data_off, int data_len, void *cmd_buf, eTexFormat format, const int res[2],
                 int out_pos[3], int border);
    bool Free(const int pos[3]);

#if defined(USE_VK_RENDER)
    eResState resource_state = eResState::Undefined;
#endif
  private:
    ApiContext *api_ctx_ = nullptr;
    int mip_count_ = 0;
    int layer_count_ = 0;
    eTexFormat format_ = eTexFormat::Undefined;
    eTexFilter filter_ = eTexFilter::NoFilter;
#if defined(USE_VK_RENDER)
    VkImage img_ = VK_NULL_HANDLE;
    VkDeviceMemory mem_ = VK_NULL_HANDLE;
    VkImageView img_view_ = VK_NULL_HANDLE;
    Sampler sampler_;
#elif defined(USE_GL_RENDER)
    uint32_t tex_id_ = 0xffffffff;
#endif

    TextureSplitter splitters_[MaxTextureCount];
};
} // namespace Ren