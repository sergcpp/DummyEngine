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
    TextureAtlas(TextureAtlas &&rhs);

    TextureAtlas &operator=(const TextureAtlas &rhs) = delete;
    TextureAtlas &operator=(TextureAtlas &&rhs);

    int resx() const { return splitter_.resx(); }
    int resy() const { return splitter_.resy(); }
    uint32_t tex_id(int i) const { return tex_ids_[i]; }

    int Allocate(const void **data, const eTexColorFormat *format, const int res[2], int pos[2], int border);
    bool Free(const int pos[2]);

    // create mipmaps, compress etc.
    void Finalize();

private:
    eTexColorFormat formats_[MaxTextureCount] = { Undefined, Undefined, Undefined, Undefined,
                                                  Undefined, Undefined, Undefined, Undefined };
    eTexFilter filter_;
#if defined(USE_GL_RENDER)
    uint32_t tex_ids_[MaxTextureCount] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                           0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
#endif

    TextureSplitter splitter_;
};
}