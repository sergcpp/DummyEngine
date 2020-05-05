#pragma once

#include "Texture.h"
#include "TextureSplitter.h"

namespace Ren {
class TextureAtlas {
  public:
    static const int MaxTextureCount = 8;

    TextureAtlas() : splitter_(0, 0) {}
    TextureAtlas(int w, int h, int min_res, const eTexFormat *formats,
                 const uint32_t *flags, eTexFilter filter, ILog *log);
    ~TextureAtlas();

    TextureAtlas(const TextureAtlas &rhs) = delete;
    TextureAtlas(TextureAtlas &&rhs) noexcept;

    TextureAtlas &operator=(const TextureAtlas &rhs) = delete;
    TextureAtlas &operator=(TextureAtlas &&rhs) noexcept;

    int resx() const { return splitter_.resx(); }
    int resy() const { return splitter_.resy(); }
    uint32_t tex_id(int i) const { return tex_ids_[i]; }

    int AllocateRegion(const int res[2], int out_pos[2]);
    void InitRegion(const void *data, int data_len, eTexFormat format, uint32_t flags,
                    int layer, int level, const int pos[2], const int res[2], ILog *log);

    bool Free(const int pos[2]);

    // create mipmaps, compress etc.
    void Finalize();

  private:
    eTexFormat formats_[MaxTextureCount] = {eTexFormat::Undefined, eTexFormat::Undefined,
                                            eTexFormat::Undefined, eTexFormat::Undefined,
                                            eTexFormat::Undefined, eTexFormat::Undefined,
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
    TextureAtlasArray(int w, int h, int layer_count, eTexFormat format,
                      eTexFilter filter);
    ~TextureAtlasArray();

    TextureAtlasArray(const TextureAtlasArray &rhs) = delete;
    TextureAtlasArray(TextureAtlasArray &&rhs) noexcept;

    TextureAtlasArray &operator=(const TextureAtlasArray &rhs) = delete;
    TextureAtlasArray &operator=(TextureAtlasArray &&rhs) noexcept;

    int resx() const { return splitters_[0].resx(); }
    int resy() const { return splitters_[0].resy(); }
    uint32_t tex_id() const { return tex_id_; }

    int Allocate(const void *data, eTexFormat format, const int res[2], int out_pos[3],
                 int border);
    bool Free(const int pos[3]);

  private:
    int layer_count_ = 0;
    eTexFormat format_ = eTexFormat::Undefined;
    eTexFilter filter_ = eTexFilter::NoFilter;
#if defined(USE_GL_RENDER)
    uint32_t tex_id_ = 0xffffffff;
#endif

    TextureSplitter splitters_[MaxTextureCount];
};
} // namespace Ren