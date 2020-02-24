#include "TextureAtlas.h"

#include "GL.h"
#include "Utils.h"

Ren::TextureAtlas::TextureAtlas(const int w, const int h, const int min_res,
                                const eTexFormat *formats, const uint32_t *flags,
                                eTexFilter filter, ILog *log)
    : splitter_(w, h) {
    filter_ = filter;

    const int mip_count = CalcMipCount(w, h, min_res, filter);

    for (int i = 0; i < MaxTextureCount; i++) {
        if (formats[i] == eTexFormat::Undefined) {
            break;
        }

        const GLenum compressed_tex_format =
#if !defined(__ANDROID__)
            (flags[i] & TexSRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                                 : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
            (flags[i] & TexSRGB) ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
                                 : GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif

        formats_[i] = formats[i];

        GLuint tex_id;
        glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);

        GLenum internal_format;

        const int blank_block_res = 64;
        uint8_t blank_block[blank_block_res * blank_block_res * 4] = {};
        if (formats[i] == Ren::eTexFormat::Compressed) {
            for (int j = 0; j < (blank_block_res / 4) * (blank_block_res / 4) * 16;) {
#if defined(__ANDROID__)
                memcpy(&blank_block[j], Ren::_blank_ASTC_block_4x4,
                       Ren::_blank_ASTC_block_4x4_len);
                j += Ren::_blank_ASTC_block_4x4_len;
#else
                memcpy(&blank_block[j], Ren::_blank_DXT5_block_4x4,
                       Ren::_blank_DXT5_block_4x4_len);
                j += Ren::_blank_DXT5_block_4x4_len;
#endif
            }
            internal_format = compressed_tex_format;
        } else {
            internal_format = GLInternalFormatFromTexFormat(formats_[i]);
        }

        if (formats[i] == Ren::eTexFormat::Compressed) {
            ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count,
                                        compressed_tex_format, w, h);
        } else {
            ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count,
                                        GLInternalFormatFromTexFormat(formats_[i]), w, h);
        }

        for (int level = 0; level < mip_count; level++) {
            const int _w = int((unsigned)w >> (unsigned)level),
                      _h = int((unsigned)h >> (unsigned)level),
                      _init_res = std::min(blank_block_res, std::min(_w, _h));
            for (int y_off = 0; y_off < _h; y_off += blank_block_res) {
                const int buf_len =
#if defined(__ANDROID__)
                    // TODO: '+ y_off' fixes an error on Qualcomm (wtf ???)
                    (_init_res / 4) * ((_init_res + y_off) / 4) * 16;
#else
                    (_init_res / 4) * (_init_res / 4) * 16;
#endif

                for (int x_off = 0; x_off < _w; x_off += blank_block_res) {
                    if (formats[i] == Ren::eTexFormat::Compressed) {
                        ren_glCompressedTextureSubImage2D_Comp(
                            GL_TEXTURE_2D, tex_id, level, x_off, y_off, _init_res,
                            _init_res, internal_format, buf_len, blank_block);
                    } else {
                        ren_glTextureSubImage2D_Comp(
                            GL_TEXTURE_2D, tex_id, level, x_off, y_off, _init_res,
                            _init_res, internal_format, GL_UNSIGNED_BYTE, blank_block);
                    }
                }
            }
        }

        const float anisotropy = 4.0f;
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                     anisotropy);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[(size_t)filter_]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[(size_t)filter_]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S,
                                     g_gl_wrap_mode[(size_t)filter]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T,
                                     g_gl_wrap_mode[(size_t)filter]);

        CheckError("create texture", log);

        tex_ids_[i] = (uint32_t)tex_id;
    }
}

Ren::TextureAtlas::~TextureAtlas() {
    for (const uint32_t tex_id : tex_ids_) {
        if (tex_id != 0xffffffff) {
            auto _tex_id = (GLuint)tex_id;
            glDeleteTextures(1, &_tex_id);
        }
    }
}

Ren::TextureAtlas::TextureAtlas(TextureAtlas &&rhs) noexcept
    : splitter_(std::move(rhs.splitter_)), filter_(rhs.filter_) {
    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = eTexFormat::Undefined;

        tex_ids_[i] = rhs.tex_ids_[i];
        rhs.tex_ids_[i] = 0xffffffff;
    }
}

Ren::TextureAtlas &Ren::TextureAtlas::operator=(TextureAtlas &&rhs) noexcept {
    filter_ = rhs.filter_;

    for (int i = 0; i < MaxTextureCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = eTexFormat::Undefined;

        if (tex_ids_[i] != 0xffffffff) {
            auto tex_id = (GLuint)tex_ids_[i];
            glDeleteTextures(1, &tex_id);
        }
        tex_ids_[i] = rhs.tex_ids_[i];
        rhs.tex_ids_[i] = 0xffffffff;
    }

    splitter_ = std::move(rhs.splitter_);
    return (*this);
}

int Ren::TextureAtlas::AllocateRegion(const int res[2], int out_pos[2]) {
    const int index = splitter_.Allocate(res, out_pos);
    return index;
}

void Ren::TextureAtlas::InitRegion(const void *data, const int data_len,
                                   const eTexFormat format, const uint32_t flags,
                                   const int layer, const int level, const int pos[2],
                                   const int res[2], ILog *log) {
#ifndef NDEBUG
    if (level == 0) {
        int _res[2];
        int rc = splitter_.FindNode(pos, _res);
        assert(rc != -1);
        assert(_res[0] == res[0] && _res[1] == res[1]);
    }
#endif

    if (format == Ren::eTexFormat::Compressed) {
        const GLenum tex_format =
#if !defined(__ANDROID__)
            (flags & TexSRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                              : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
            (flags & TexSRGB) ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
                              : GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif
        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, (GLuint)tex_ids_[layer],
                                               level, pos[0], pos[1], res[0], res[1],
                                               tex_format, data_len, data);
    } else {
        ren_glTextureSubImage2D_Comp(
            GL_TEXTURE_2D, (GLuint)tex_ids_[layer], level, pos[0], pos[1], res[0], res[1],
            GLFormatFromTexFormat(format), GLTypeFromTexFormat(format), data);
    }
    CheckError("init sub image", log);
}

bool Ren::TextureAtlas::Free(const int pos[2]) {
    // TODO: fill with black in debug
    return splitter_.Free(pos);
}

void Ren::TextureAtlas::Finalize() {
    if (filter_ == eTexFilter::Trilinear || filter_ == eTexFilter::Bilinear) {
        for (int i = 0; i < MaxTextureCount && (formats_[i] != eTexFormat::Undefined);
             i++) {
            if (formats_[i] != Ren::eTexFormat::Compressed) {
                ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_2D, (GLuint)tex_ids_[i]);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////

Ren::TextureAtlasArray::TextureAtlasArray(int w, int h, int layer_count,
                                          const eTexFormat format, eTexFilter filter)
    : layer_count_(layer_count), format_(format), filter_(filter) {
    GLuint tex_id;
    ren_glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &tex_id);

    const int mip_count = Ren::CalcMipCount(w, h, 1, filter);
    ren_glTextureStorage3D_Comp(GL_TEXTURE_2D_ARRAY, tex_id, mip_count,
                                GLInternalFormatFromTexFormat(format), w, h, layer_count);

    ren_glTextureParameteri_Comp(GL_TEXTURE_2D_ARRAY, tex_id, GL_TEXTURE_MIN_FILTER,
                                 g_gl_min_filter[(size_t)filter_]);
    ren_glTextureParameteri_Comp(GL_TEXTURE_2D_ARRAY, tex_id, GL_TEXTURE_MAG_FILTER,
                                 g_gl_mag_filter[(size_t)filter_]);

    tex_id_ = (uint32_t)tex_id;

    for (int i = 0; i < layer_count; i++) {
        splitters_[i] = TextureSplitter{w, h};
    }
}

Ren::TextureAtlasArray::~TextureAtlasArray() {
    if (tex_id_ != 0xffffffff) {
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }
}

Ren::TextureAtlasArray::TextureAtlasArray(TextureAtlasArray &&rhs) noexcept {
    layer_count_ = rhs.layer_count_;
    rhs.layer_count_ = 0;

    format_ = rhs.format_;
    rhs.format_ = eTexFormat::Undefined;

    filter_ = rhs.filter_;

    tex_id_ = rhs.tex_id_;
    rhs.tex_id_ = 0xffffffff;

    for (int i = 0; i < layer_count_; i++) {
        splitters_[i] = std::move(rhs.splitters_[i]);
    }
}

Ren::TextureAtlasArray &
Ren::TextureAtlasArray::operator=(TextureAtlasArray &&rhs) noexcept {
    layer_count_ = rhs.layer_count_;
    rhs.layer_count_ = 0;

    format_ = rhs.format_;
    rhs.format_ = eTexFormat::Undefined;

    filter_ = rhs.filter_;

    if (tex_id_ != 0xffffffff) {
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }

    tex_id_ = rhs.tex_id_;
    rhs.tex_id_ = 0xffffffff;

    for (int i = 0; i < layer_count_; i++) {
        splitters_[i] = std::move(rhs.splitters_[i]);
    }

    return (*this);
}

int Ren::TextureAtlasArray::Allocate(const void *data, const eTexFormat format,
                                     const int res[2], int out_pos[3], const int border) {
    const int alloc_res[] = {res[0] < splitters_[0].resx() ? res[0] + border : res[0],
                             res[1] < splitters_[1].resy() ? res[1] + border : res[1]};

    for (int i = 0; i < layer_count_; i++) {
        const int index = splitters_[i].Allocate(alloc_res, out_pos);
        if (index != -1) {
            out_pos[2] = i;

            ren_glTextureSubImage3D_Comp(GL_TEXTURE_2D_ARRAY, (GLuint)tex_id_, 0,
                                         out_pos[0], out_pos[1], out_pos[2], res[0],
                                         res[1], 1, GLFormatFromTexFormat(format),
                                         GLTypeFromTexFormat(format), data);
            return index;
        }
    }

    return -1;
}

bool Ren::TextureAtlasArray::Free(const int pos[3]) {
    // TODO: fill with black in debug
    return splitters_[pos[2]].Free(pos);
}
