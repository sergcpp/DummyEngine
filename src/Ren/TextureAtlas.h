#pragma once

#include "Sampler.h"
#include "Resource.h"
#include "Texture.h"
#include "TextureSplitter.h"

namespace Ren {
class TextureAtlas {
  public:
    static const int MaxTextureCount = 8;

    TextureAtlas() : splitter_(0, 0) {}
    TextureAtlas(int w, int h, int min_res, const eTexFormat *formats, const uint32_t *flags, eTexFilter filter,
                 ILog *log);
    ~TextureAtlas();

    TextureAtlas(const TextureAtlas &rhs) = delete;
    TextureAtlas(TextureAtlas &&rhs) noexcept;

    TextureAtlas &operator=(const TextureAtlas &rhs) = delete;
    TextureAtlas &operator=(TextureAtlas &&rhs) noexcept;

    int resx() const { return splitter_.resx(); }
    int resy() const { return splitter_.resy(); }
#if defined(USE_GL_RENDER)
    uint32_t tex_id(const int i) const { return tex_ids_[i]; }
#endif

    int AllocateRegion(const int res[2], int out_pos[2]);
    void InitRegion(const void *data, int data_len, eTexFormat format, uint32_t flags, int layer, int level,
                    const int pos[2], const int res[2], ILog *log);

    bool Free(const int pos[2]);

    // create mipmaps, compress etc.
    void Finalize();

  private:
    eTexFormat formats_[MaxTextureCount] = {eTexFormat::Undefined, eTexFormat::Undefined, eTexFormat::Undefined,
                                            eTexFormat::Undefined, eTexFormat::Undefined, eTexFormat::Undefined,
                                            eTexFormat::Undefined, eTexFormat::Undefined};
    eTexFilter filter_ = eTexFilter::NoFilter;
#if defined(USE_GL_RENDER)
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

    int Allocate(const void *data, eTexFormat format, const int res[2], int out_pos[3], int border);
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