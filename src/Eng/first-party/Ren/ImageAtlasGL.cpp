#include "ImageAtlas.h"

#include "Context.h"
#include "GL.h"
#include "Utils.h"

namespace Ren {
extern const uint32_t g_min_filter_gl[];
extern const uint32_t g_mag_filter_gl[];
extern const uint32_t g_wrap_mode_gl[];
} // namespace Ren

Ren::ImageAtlas::ImageAtlas(ApiContext *api_ctx, const int w, const int h, const int min_res, const int mip_count,
                            const eFormat formats[], const Bitmask<eImgFlags> flags[], eFilter filter, ILog *log)
    : splitter_(w, h) {
    filter_ = filter;

    for (int i = 0; i < MaxImageCount; i++) {
        if (formats[i] == eFormat::Undefined) {
            break;
        }

        const GLenum compressed_tex_format =
#if !defined(__ANDROID__)
            /*(flags[i] & eTexFlags::SRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT :*/
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
            /*(flags[i] & eTexFlags::SRGB) ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR :*/
            GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif

        formats_[i] = formats[i];

        GLuint tex_id;
        glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);

        GLenum internal_format;

        const int blank_block_res = 16;
        uint8_t blank_block[blank_block_res * blank_block_res * 4] = {};
        if (IsCompressedFormat(formats[i])) {
            for (int j = 0; j < (blank_block_res / 4) * (blank_block_res / 4) * 16;) {
#if defined(__ANDROID__)
                memcpy(&blank_block[j], _blank_ASTC_block_4x4, _blank_ASTC_block_4x4_len);
                j += _blank_ASTC_block_4x4_len;
#else
                memcpy(&blank_block[j], _blank_DXT5_block_4x4, _blank_DXT5_block_4x4_len);
                j += _blank_DXT5_block_4x4_len;
#endif
            }
            internal_format = compressed_tex_format;
        } else {
            internal_format = GLInternalFormatFromFormat(formats_[i]);
        }

        ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count, internal_format, w, h);

        for (int level = 0; level < mip_count; level++) {
            const int _w = int(unsigned(w) >> unsigned(level)), _h = int(unsigned(h) >> unsigned(level)),
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
                    if (IsCompressedFormat(formats[i])) {
                        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, level, x_off, y_off, _init_res,
                                                               _init_res, internal_format, buf_len, blank_block);
                    } else {
                        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, level, x_off, y_off, _init_res, _init_res,
                                                     internal_format, GL_UNSIGNED_BYTE, blank_block);
                    }
                }
            }
        }

        const float anisotropy = 4;
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER, g_min_filter_gl[size_t(filter_)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER, g_mag_filter_gl[size_t(filter_)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S, g_wrap_mode_gl[size_t(filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T, g_wrap_mode_gl[size_t(filter)]);

        CheckError("create texture", log);

        tex_ids_[i] = uint32_t(tex_id);
    }
}

Ren::ImageAtlas::~ImageAtlas() {
    for (const uint32_t tex_id : tex_ids_) {
        if (tex_id != 0xffffffff) {
            auto _tex_id = GLuint(tex_id);
            glDeleteTextures(1, &_tex_id);
        }
    }
}

Ren::ImageAtlas::ImageAtlas(ImageAtlas &&rhs) noexcept : splitter_(std::move(rhs.splitter_)), filter_(rhs.filter_) {
    for (int i = 0; i < MaxImageCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = eFormat::Undefined;

        tex_ids_[i] = rhs.tex_ids_[i];
        rhs.tex_ids_[i] = 0xffffffff;
    }
}

Ren::ImageAtlas &Ren::ImageAtlas::operator=(ImageAtlas &&rhs) noexcept {
    filter_ = rhs.filter_;

    for (int i = 0; i < MaxImageCount; i++) {
        formats_[i] = rhs.formats_[i];
        rhs.formats_[i] = eFormat::Undefined;

        if (tex_ids_[i] != 0xffffffff) {
            auto tex_id = GLuint(tex_ids_[i]);
            glDeleteTextures(1, &tex_id);
        }
        tex_ids_[i] = rhs.tex_ids_[i];
        rhs.tex_ids_[i] = 0xffffffff;
    }

    splitter_ = std::move(rhs.splitter_);
    return (*this);
}

int Ren::ImageAtlas::AllocateRegion(const int res[2], int out_pos[2]) {
    const int index = splitter_.Allocate(res, out_pos);
    return index;
}

void Ren::ImageAtlas::InitRegion(const Buffer &sbuf, const int data_off, const int data_len, CommandBuffer cmd_buf,
                                 const eFormat format, const Bitmask<eImgFlags> flags, const int layer, const int level,
                                 const int pos[2], const int res[2], ILog *log) {
#ifndef NDEBUG
    if (level == 0) {
        int _res[2];
        int rc = splitter_.FindNode(pos, _res);
        assert(rc != -1);
        assert(_res[0] == res[0] && _res[1] == res[1]);
    }
#endif

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    if (IsCompressedFormat(format)) {
        const GLenum tex_format =
#if !defined(__ANDROID__)
            /*(flags & eTexFlags::SRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT :*/ GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
            /*(flags & eTexFlags::SRGB) ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR :*/ GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif
        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(tex_ids_[layer]), level, pos[0], pos[1], res[0],
                                               res[1], tex_format, data_len,
                                               reinterpret_cast<const void *>(uintptr_t(data_off)));
    } else {
        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(tex_ids_[layer]), level, pos[0], pos[1], res[0], res[1],
                                     GLFormatFromFormat(format), GLTypeFromFormat(format),
                                     reinterpret_cast<const void *>(uintptr_t(data_off)));
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    CheckError("init sub image", log);
}

bool Ren::ImageAtlas::Free(const int pos[2]) {
    // TODO: fill with black in debug
    return splitter_.Free(pos);
}

void Ren::ImageAtlas::Finalize(CommandBuffer cmd_buf) {
    if (filter_ == eFilter::Trilinear || filter_ == eFilter::Bilinear) {
        for (int i = 0; i < MaxImageCount && (formats_[i] != eFormat::Undefined); i++) {
            if (!IsCompressedFormat(formats_[i])) {
                ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_2D, GLuint(tex_ids_[i]));
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

Ren::ImageAtlasArray::ImageAtlasArray(ApiContext *api_ctx, const std::string_view name, const int w, const int h,
                                      const int layer_count, const int mip_count, const eFormat format,
                                      const eFilter filter, const Bitmask<eImgUsage> usage)
    : api_ctx_(api_ctx), name_(name), w_(w), h_(h), layer_count_(layer_count), format_(format), filter_(filter) {
    GLuint tex_id;
    ren_glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &tex_id);

    ren_glTextureStorage3D_Comp(GL_TEXTURE_2D_ARRAY, tex_id, mip_count, GLInternalFormatFromFormat(format), w, h,
                                layer_count);

    ren_glTextureParameteri_Comp(GL_TEXTURE_2D_ARRAY, tex_id, GL_TEXTURE_MIN_FILTER, g_min_filter_gl[size_t(filter_)]);
    ren_glTextureParameteri_Comp(GL_TEXTURE_2D_ARRAY, tex_id, GL_TEXTURE_MAG_FILTER, g_mag_filter_gl[size_t(filter_)]);

    tex_id_ = uint32_t(tex_id);

    splitters_.resize(layer_count, ImageSplitter{w, h});
}

void Ren::ImageAtlasArray::Free() {
    if (tex_id_ != 0xffffffff) {
        auto tex_id = GLuint(tex_id_);
        glDeleteTextures(1, &tex_id);
        tex_id_ = 0xffffffff;
    }
}

void Ren::ImageAtlasArray::FreeImmediate() { Free(); }

Ren::ImageAtlasArray &Ren::ImageAtlasArray::operator=(ImageAtlasArray &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    if (tex_id_ != 0xffffffff) {
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }

    mip_count_ = std::exchange(rhs.mip_count_, 0);
    layer_count_ = std::exchange(rhs.layer_count_, 0);
    format_ = std::exchange(rhs.format_, eFormat::Undefined);
    filter_ = std::exchange(rhs.filter_, eFilter::Nearest);

    resource_state = std::exchange(rhs.resource_state, eResState::Undefined);

    tex_id_ = std::exchange(rhs.tex_id_, 0xffffffff);

    splitters_ = std::move(rhs.splitters_);

    return (*this);
}

void Ren::ImageAtlasArray::SetSubImage(const int level, const int layer, const int offsetx, const int offsety,
                                       const int sizex, const int sizey, const eFormat format, const Buffer &sbuf,
                                       const int data_off, const int data_len, void *) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    ren_glTextureSubImage3D_Comp(GL_TEXTURE_2D_ARRAY, GLuint(tex_id_), level, offsetx, offsety, layer, sizex, sizey, 1,
                                 GLFormatFromFormat(format), GLTypeFromFormat(format),
                                 reinterpret_cast<const void *>(uintptr_t(data_off)));

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void Ren::ImageAtlasArray::Clear(const float rgba[4], void *) {
    glClearTexImage(GLuint(tex_id_), 0, GL_RGBA, GL_FLOAT, rgba);
}