#include "TextureAtlas.h"

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#endif

TextureAtlas::TextureAtlas(int w, int h, const Ren::eTexColorFormat *formats, Ren::eTexFilter filter)
        : splitter_(w, h) {
    filter_ = filter;

    for (int i = 0; i < MaxTextureCount; i++) {
        if (formats[i] == Ren::Undefined) break;

        formats_[i] = formats[i];

#if defined(USE_GL_RENDER)
        GLuint tex_id;
        glGenTextures(1, &tex_id);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_id);

        glTexImage2D(GL_TEXTURE_2D, 0, Ren::GLInternalFormatFromTexFormat(formats_[i]), w, h, 0,
                     Ren::GLFormatFromTexFormat(formats_[i]), Ren::GLTypeFromTexFormat(formats_[i]), nullptr);

        const float anisotropy = 4.0f;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

        if (filter_ == Ren::NoFilter) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        } else if (filter_ == Ren::Bilinear) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else if (filter_ == Ren::Trilinear) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else if (filter_ == Ren::BilinearNoMipmap) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        tex_ids_[i] = (uint32_t)tex_id;
#endif
    }
}

TextureAtlas::~TextureAtlas() {
    for (int i = 0; i < MaxTextureCount; i++) {
#if defined(USE_GL_RENDER)
        if (tex_ids_[i] != 0xffffffff) {
            GLuint tex_id = (GLuint)tex_ids_[i];
            glDeleteTextures(1, &tex_id);
        }
#endif
    }
}

TextureAtlas::TextureAtlas(TextureAtlas &&rhs) : splitter_(std::move(rhs.splitter_)), filter_(rhs.filter_) {
    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = Ren::Undefined;

#if defined(USE_GL_RENDER)
        tex_ids_[i] = rhs.tex_ids_[i];
        rhs.tex_ids_[i] = 0xffffffff;
#endif
    }
}

TextureAtlas &TextureAtlas::operator=(TextureAtlas &&rhs) {
    filter_ = rhs.filter_;

    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = Ren::Undefined;

#if defined(USE_GL_RENDER)
        if (tex_ids_[i] != 0xffffffff) {
            GLuint tex_id = (GLuint)tex_ids_[i];
            glDeleteTextures(1, &tex_id);
        }
        tex_ids_[i] = rhs.tex_ids_[i];
        rhs.tex_ids_[i] = 0xffffffff;
#endif
    }

    splitter_ = std::move(rhs.splitter_);
    return (*this);
}

int TextureAtlas::Allocate(const void **data, const Ren::eTexColorFormat *format, const int res[2], int pos[2], int border) {
    const int alloc_res[] = { res[0] + border, res[1] + border };

    int index = splitter_.Allocate(alloc_res, pos);
    if (index != -1) {
#if defined(USE_GL_RENDER)
        for (int i = 0; i < MaxTextureCount; i++) {
            if (!data[i]) break;

            glBindTexture(GL_TEXTURE_2D, (GLuint)tex_ids_[i]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, pos[0], pos[1], res[0], res[1],
                            Ren::GLFormatFromTexFormat(format[i]), Ren::GLTypeFromTexFormat(format[i]), data[i]);
        }
#endif
    }
    return index;
}

bool TextureAtlas::Free(const int pos[2]) {
    // TODO: fill with black in debug
    return splitter_.Free(pos);
}

void TextureAtlas::Finalize() {
    if (filter_ == Ren::Trilinear || filter_ == Ren::Bilinear) {
        for (int i = 0; i < MaxTextureCount; i++) {
            if (formats_[i] == Ren::Undefined) break;

#if defined(USE_GL_RENDER)
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, (GLuint)tex_ids_[i]);

            //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
            glGenerateMipmap(GL_TEXTURE_2D);
#endif
        }
    }
}