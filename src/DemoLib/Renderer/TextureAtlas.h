#pragma once

#include <Ren/TextureSplitter.h>

class TextureAtlas {
    Ren::Texture2DParams params_;
#if defined(USE_GL_RENDER)
    uint32_t tex_id_ = 0xffffffff;
#endif

    Ren::TextureSplitter splitter_;
public:
    TextureAtlas() : splitter_(0, 0) {}
    explicit TextureAtlas(const Ren::Texture2DParams &p);
    ~TextureAtlas();

    TextureAtlas(const TextureAtlas &rhs) = delete;
    TextureAtlas(TextureAtlas &&rhs);

    TextureAtlas &operator=(const TextureAtlas &rhs) = delete;
    TextureAtlas &operator=(TextureAtlas &&rhs);

    const Ren::Texture2DParams &params() const { return params_; }
    uint32_t tex_id() const { return tex_id_; }

    int Allocate(const void *data, Ren::eTexColorFormat format, const int res[2], int pos[2], int border);
    bool Free(const int pos[2]);

    // create mipmaps, compress etc.
    void Finalize();
};