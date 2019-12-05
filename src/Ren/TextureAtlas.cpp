#include "TextureAtlas.h"

#if defined(USE_GL_RENDER)
#include "GL.h"
#endif

Ren::TextureAtlas::TextureAtlas(int w, int h, const eTexColorFormat *formats, eTexFilter filter)
        : splitter_(w, h) {
    filter_ = filter;

    for (int i = 0; i < MaxTextureCount; i++) {
        if (formats[i] == Undefined) break;

        formats_[i] = formats[i];

#if defined(USE_GL_RENDER)
        GLuint tex_id;
        glGenTextures(1, &tex_id);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_id);

        glTexImage2D(GL_TEXTURE_2D, 0, GLInternalFormatFromTexFormat(formats_[i]), w, h, 0,
                        GLFormatFromTexFormat(formats_[i]), GLTypeFromTexFormat(formats_[i]), nullptr);

        const float anisotropy = 4.0f;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

        if (filter_ == NoFilter) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        } else if (filter_ == Bilinear) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else if (filter_ == Trilinear) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else if (filter_ == BilinearNoMipmap) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        tex_ids_[i] = (uint32_t)tex_id;
#endif
    }
}

Ren::TextureAtlas::~TextureAtlas() {
    for (int i = 0; i < MaxTextureCount; i++) {
#if defined(USE_GL_RENDER)
        if (tex_ids_[i] != 0xffffffff) {
            GLuint tex_id = (GLuint)tex_ids_[i];
            glDeleteTextures(1, &tex_id);
        }
#endif
    }
}

Ren::TextureAtlas::TextureAtlas(TextureAtlas &&rhs) noexcept
        : splitter_(std::move(rhs.splitter_)), filter_(rhs.filter_) {
    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = Undefined;

#if defined(USE_GL_RENDER)
        tex_ids_[i] = rhs.tex_ids_[i];
        rhs.tex_ids_[i] = 0xffffffff;
#endif
    }
}

Ren::TextureAtlas &Ren::TextureAtlas::operator=(TextureAtlas &&rhs) noexcept {
    filter_ = rhs.filter_;

    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = Undefined;

#if defined(USE_GL_RENDER)
        if (tex_ids_[i] != 0xffffffff) {
            auto tex_id = (GLuint)tex_ids_[i];
            glDeleteTextures(1, &tex_id);
        }
        tex_ids_[i] = rhs.tex_ids_[i];
        rhs.tex_ids_[i] = 0xffffffff;
#endif
    }

    splitter_ = std::move(rhs.splitter_);
    return (*this);
}

int Ren::TextureAtlas::Allocate(const void **data, const eTexColorFormat *format, const int res[2], int out_pos[2], int border) {
    const int alloc_res[] = { res[0] + border, res[1] + border };

    int index = splitter_.Allocate(alloc_res, out_pos);
    if (index != -1) {
#if defined(USE_GL_RENDER)
        for (int i = 0; i < MaxTextureCount; i++) {
            if (!data[i]) break;

            glBindTexture(GL_TEXTURE_2D, (GLuint)tex_ids_[i]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, out_pos[0], out_pos[1], res[0], res[1],
                            GLFormatFromTexFormat(format[i]), GLTypeFromTexFormat(format[i]), data[i]);
        }
#endif
    }
    return index;
}

bool Ren::TextureAtlas::Free(const int pos[2]) {
    // TODO: fill with black in debug
    return splitter_.Free(pos);
}

void Ren::TextureAtlas::Finalize() {
    if (filter_ == Trilinear || filter_ == Bilinear) {
        for (int i = 0; i < MaxTextureCount; i++) {
            if (formats_[i] == Undefined) break;

#if defined(USE_GL_RENDER)
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, (GLuint)tex_ids_[i]);

            //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
            glGenerateMipmap(GL_TEXTURE_2D);
#endif
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

Ren::TextureAtlasArray::TextureAtlasArray(int w, int h, int layer_count, const eTexColorFormat format, eTexFilter filter)
        : layer_count_(layer_count), format_(format), filter_(filter) {
#if defined(USE_GL_RENDER)
    GLuint tex_id;
    glGenTextures(1, &tex_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GLInternalFormatFromTexFormat(format), w, h, layer_count,
                 0, GLFormatFromTexFormat(format), GLTypeFromTexFormat(format), nullptr);

    if (filter_ == NoFilter) {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else if (filter_ == Bilinear) {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (filter_ == Trilinear) {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (filter_ == BilinearNoMipmap) {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    tex_id_ = (uint32_t)tex_id;
#endif

    for (int i = 0; i < layer_count; i++) {
        splitters_[i] = TextureSplitter{ w, h };
    }
}

Ren::TextureAtlasArray::~TextureAtlasArray() {
#if defined(USE_GL_RENDER)
    if (tex_id_ != 0xffffffff) {
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }
#endif
}

Ren::TextureAtlasArray::TextureAtlasArray(TextureAtlasArray &&rhs) noexcept {
    layer_count_ = rhs.layer_count_;
    rhs.layer_count_ = 0;

    format_ = rhs.format_;
    rhs.format_ = Undefined;

    filter_ = rhs.filter_;

#if defined(USE_GL_RENDER)
    tex_id_ = rhs.tex_id_;
    rhs.tex_id_ = 0xffffffff;
#endif

    for (int i = 0; i < layer_count_; i++) {
        splitters_[i] = std::move(rhs.splitters_[i]);
    }
}

Ren::TextureAtlasArray &Ren::TextureAtlasArray::operator=(TextureAtlasArray &&rhs) noexcept {
    layer_count_ = rhs.layer_count_;
    rhs.layer_count_ = 0;

    format_ = rhs.format_;
    rhs.format_ = Undefined;

    filter_ = rhs.filter_;

#if defined(USE_GL_RENDER)
    if (tex_id_ != 0xffffffff) {
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }

    tex_id_ = rhs.tex_id_;
    rhs.tex_id_ = 0xffffffff;
#endif

    for (int i = 0; i < layer_count_; i++) {
        splitters_[i] = std::move(rhs.splitters_[i]);
    }

    return (*this);
}

int Ren::TextureAtlasArray::Allocate(const void *data, const eTexColorFormat format, const int res[2], int out_pos[3], int border) {
    const int alloc_res[] = { res[0] + border, res[1] + border };

    for (int i = 0; i < layer_count_; i++) {
        int index = splitters_[i].Allocate(alloc_res, out_pos);
        if (index != -1) {
            out_pos[2] = i;

#if defined(USE_GL_RENDER)
            glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)tex_id_);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, out_pos[0], out_pos[1], out_pos[2], res[0], res[1], 1,
                            GLFormatFromTexFormat(format), GLTypeFromTexFormat(format), data);

#endif
            return index;
        }
    }

    return -1;
}

bool Ren::TextureAtlasArray::Free(const int pos[3]) {
    // TODO: fill with black in debug
    return splitters_[pos[2]].Free(pos);
}
