#include "TextureAtlas.h"

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#endif

TextureAtlas::TextureAtlas(const Ren::Texture2DParams &p) : params_(p), splitter_(p.w, p.h) {
#if defined(USE_GL_RENDER)
    GLuint tex_id;

    glGenTextures(1, &tex_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    if (p.format == Ren::RawRGBA8888) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p.w, p.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    } else if (p.format == Ren::RawRGB888) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, p.w, p.h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    } else if (p.format == Ren::RawLUM8) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, p.w, p.h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
    } else if (p.format == Ren::RawRGB16F) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, p.w, p.h, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
    } else if (p.format == Ren::RawRGB32F) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, p.w, p.h, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    const float anisotropy = 4.0f;
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

    if (p.filter == Ren::NoFilter) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else if (p.filter == Ren::Bilinear) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (p.filter == Ren::Trilinear) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (p.filter == Ren::BilinearNoMipmap) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (p.repeat == Ren::Repeat) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else if (p.repeat == Ren::ClampToEdge) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    tex_id_ = (uint32_t)tex_id;
#endif
}

TextureAtlas::~TextureAtlas() {
#if defined(USE_GL_RENDER)
    if (tex_id_ != 0xffffffff) {
        GLuint tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }
#endif
}

TextureAtlas::TextureAtlas(TextureAtlas &&rhs) : params_(rhs.params_), tex_id_(rhs.tex_id_), splitter_(std::move(rhs.splitter_)) {
    rhs.tex_id_ = 0xffffffff;
}

TextureAtlas &TextureAtlas::operator=(TextureAtlas &&rhs) {
    params_ = rhs.params_;
#if defined(USE_GL_RENDER)
    if (tex_id_ != 0xffffffff) {
        GLuint tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }
    tex_id_ = rhs.tex_id_;
    rhs.tex_id_ = 0xffffffff;

    splitter_ = std::move(rhs.splitter_);
#endif

    return (*this);
}

int TextureAtlas::Allocate(const void *data, Ren::eTexColorFormat format, const int res[2], int pos[2], int border) {
    const int alloc_res[] = { res[0] + border, res[1] + border };

    int index = splitter_.Allocate(alloc_res, pos);
    if (index != -1) {
#if defined(USE_GL_RENDER)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex_id_);

        glTexSubImage2D(GL_TEXTURE_2D, 0, pos[0], pos[1], res[0], res[1],
            Ren::GLFormatFromTexFormat(format), Ren::GLTypeFromTexFormat(format), data);
#endif
    }
    return index;
}

bool TextureAtlas::Free(const int pos[2]) {
    // TODO: fill with black in debug
    return splitter_.Free(pos);
}

void TextureAtlas::Finalize() {
    if (params_.filter == Ren::Trilinear || params_.filter == Ren::Bilinear) {
#if defined(USE_GL_RENDER)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex_id_);

        //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
        glGenerateMipmap(GL_TEXTURE_2D);
#endif
    }
}