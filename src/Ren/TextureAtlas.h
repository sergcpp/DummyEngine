#pragma once

#include "Texture.h"
#include "TextureSplitter.h"

namespace Ren {
class TextureAtlas {
public:
    static const int MaxTextureCount = 8;

    TextureAtlas() : splitter_(0, 0) {}
    TextureAtlas(int w, int h, const eTexColorFormat *formats, eTexFilter filter);
    ~TextureAtlas();

    TextureAtlas(const TextureAtlas &rhs) = delete;
    TextureAtlas(TextureAtlas &&rhs) noexcept;

    TextureAtlas &operator=(const TextureAtlas &rhs) = delete;
    TextureAtlas &operator=(TextureAtlas &&rhs) noexcept;

    int resx() const { return splitter_.resx(); }
    int resy() const { return splitter_.resy(); }
    uint32_t tex_id(int i) const { return tex_ids_[i]; }

    int Allocate(const void **data, const eTexColorFormat *format, const int res[2], int out_pos[2], int border);
    bool Free(const int pos[2]);

    // create mipmaps, compress etc.
    void Finalize();

private:
    eTexColorFormat formats_[MaxTextureCount] = {
        Undefined, Undefined, Undefined, Undefined,
        Undefined, Undefined, Undefined, Undefined
    };
    eTexFilter filter_ = NoFilter;
#if defined(USE_GL_RENDER)
    uint32_t tex_ids_[MaxTextureCount] = {
        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
    };
#endif

    TextureSplitter splitter_;
};

class TextureAtlasArray {
public:
    static const int MaxTextureCount = 8;

    TextureAtlasArray() = default;
    TextureAtlasArray(int w, int h, int layer_count, eTexColorFormat format, eTexFilter filter);
    ~TextureAtlasArray();

    TextureAtlasArray(const TextureAtlasArray &rhs) = delete;
    TextureAtlasArray(TextureAtlasArray &&rhs) noexcept;

    TextureAtlasArray &operator=(const TextureAtlasArray &rhs) = delete;
    TextureAtlasArray &operator=(TextureAtlasArray &&rhs) noexcept;

    int resx() const { return splitters_[0].resx(); }
    int resy() const { return splitters_[0].resy(); }
    uint32_t tex_id() const { return tex_id_; }

    int Allocate(const void *data, eTexColorFormat format, const int res[2], int out_pos[3], int border);
    bool Free(const int pos[3]);

private:
    int             layer_count_ = 0;
    eTexColorFormat format_ = Undefined;
    eTexFilter      filter_ = NoFilter;
#if defined(USE_GL_RENDER)
    uint32_t tex_id_        = 0xffffffff;
#endif

    TextureSplitter splitters_[MaxTextureCount];
};
}