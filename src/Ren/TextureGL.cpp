#include "TextureGL.h"

#include <memory>

#include "SOIL2/SOIL2.h"
#undef min
#undef max

#include "GL.h"
#include "Utils.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
const uint32_t g_gl_formats[] = {
    0xffffffff, // Undefined
    GL_RGB,     // RawRGB888
    GL_RGBA,    // RawRGBA8888
    GL_RGBA,    // RawSignedRGBA8888
    GL_RED,     // RawR32F
    GL_RED,     // RawR16F
    GL_RED,     // RawR8
    GL_RG,      // RawRG88
    GL_RGB,     // RawRGB32F
    GL_RGBA,    // RawRGBA32F
    0xffffffff, // RawRGBE8888
    GL_RGB,     // RawRGB16F
    GL_RGBA,    // RawRGBA16F
    GL_RG,      // RawRG16
    GL_RG,      // RawRG16U
    GL_RG,      // RawRG16F
    GL_RG,      // RawRG32F
    GL_RGBA,    // RawRGB10_A2
    GL_RGB,     // RawRG11F_B10F
    0xffffffff, // Compressed
    0xffffffff, // None
};
static_assert(sizeof(g_gl_formats) / sizeof(g_gl_formats[0]) ==
                  (size_t)eTexFormat::FormatCount,
              "!");

const uint32_t g_gl_internal_formats[] = {
    0xffffffff,        // Undefined
    GL_RGB8,           // RawRGB888
    GL_RGBA8,          // RawRGBA8888
    GL_RGBA8_SNORM,    // RawSignedRGBA8888
    GL_R32F,           // RawR32F
    GL_R16F,           // RawR16F
    GL_R8,             // RawR8
    GL_RG8,            // RawRG88
    GL_RGB32F,         // RawRGB32F
    GL_RGBA32F,        // RawRGBA32F
    0xffffffff,        // RawRGBE8888
    GL_RGB16F,         // RawRGB16F
    GL_RGBA16F,        // RawRGBA16F
    GL_RG16_SNORM_EXT, // RawRG16
    GL_RG16_EXT,       // RawRG16U
    GL_RG16F,          // RawRG16F
    GL_RG32F,          // RawRG32F
    GL_RGB10_A2,       // RawRGB10_A2
    GL_R11F_G11F_B10F, // RawRG11F_B10F
    0xffffffff,        // Compressed
    0xffffffff,        // None
};
static_assert(sizeof(g_gl_internal_formats) / sizeof(g_gl_internal_formats[0]) ==
                  (size_t)eTexFormat::FormatCount,
              "!");

const uint32_t g_gl_types[] = {
    0xffffffff,        // Undefined
    GL_UNSIGNED_BYTE,  // RawRGB888
    GL_UNSIGNED_BYTE,  // RawRGBA8888
    GL_BYTE,           // RawSignedRGBA8888
    GL_FLOAT,          // RawR32F
    GL_HALF_FLOAT,     // RawR16F
    GL_UNSIGNED_BYTE,  // RawR8
    GL_UNSIGNED_BYTE,  // RawRG88
    GL_FLOAT,          // RawRGB32F
    GL_FLOAT,          // RawRGBA32F
    0xffffffff,        // RawRGBE8888
    GL_HALF_FLOAT,     // RawRGB16F
    GL_HALF_FLOAT,     // RawRGBA16F
    GL_SHORT,          // RawRG16
    GL_UNSIGNED_SHORT, // RawRG16U
    GL_HALF_FLOAT,     // RawRG16F
    GL_FLOAT,          // RawRG32F
    GL_UNSIGNED_BYTE,  // RawRGB10_A2
    GL_FLOAT,          // RawRG11F_B10F
    0xffffffff,        // Compressed
    0xffffffff,        // None
};
static_assert(sizeof(g_gl_types) / sizeof(g_gl_types[0]) ==
                  (size_t)eTexFormat::FormatCount,
              "!");

const uint32_t g_gl_min_filter[] = {
    GL_NEAREST,               // NoFilter
    GL_LINEAR_MIPMAP_NEAREST, // Bilinear
    GL_LINEAR_MIPMAP_LINEAR,  // Trilinear
    GL_LINEAR,                // BilinearNoMipmap
};
static_assert(sizeof(g_gl_min_filter) / sizeof(g_gl_min_filter[0]) ==
                  (size_t)eTexFilter::FilterCount,
              "!");

const uint32_t g_gl_mag_filter[] = {
    GL_NEAREST, // NoFilter
    GL_LINEAR,  // Bilinear
    GL_LINEAR,  // Trilinear
    GL_LINEAR,  // BilinearNoMipmap
};
static_assert(sizeof(g_gl_mag_filter) / sizeof(g_gl_mag_filter[0]) ==
                  (size_t)eTexFilter::FilterCount,
              "!");

const uint32_t g_gl_wrap_mode[] = {
    GL_REPEAT,          // Repeat
    GL_CLAMP_TO_EDGE,   // ClampToEdge
    GL_CLAMP_TO_BORDER, // ClampToBorder
};
static_assert(sizeof(g_gl_wrap_mode) / sizeof(g_gl_wrap_mode[0]) ==
                  (size_t)eTexRepeat::WrapModesCount,
              "!");

const float AnisotropyLevel = 4.0f;

bool IsMainThread();
} // namespace Ren

Ren::Texture2D::Texture2D(const char *name, const void *data, int size,
                          const Texture2DParams &p, eTexLoadStatus *load_status,
                          ILog *log)
    : name_(name) {
    Init(data, size, p, load_status, log);
}

Ren::Texture2D::Texture2D(const char *name, const void *data[6], const int size[6],
                          const Texture2DParams &p, eTexLoadStatus *load_status,
                          ILog *log)
    : name_(name) {
    Init(data, size, p, load_status, log);
}

Ren::Texture2D::~Texture2D() { Free(); }

Ren::Texture2D &Ren::Texture2D::operator=(Ren::Texture2D &&rhs) noexcept {
    if (this == &rhs)
        return *this;

    Free();

    tex_id_ = rhs.tex_id_;
    rhs.tex_id_ = 0;
    params_ = rhs.params_;
    rhs.params_ = {};
    ready_ = rhs.ready_;
    rhs.ready_ = false;
    cubemap_ready_ = rhs.cubemap_ready_;
    rhs.cubemap_ready_ = 0;
    name_ = std::move(rhs.name_);

    RefCounter::operator=(std::move(rhs));

    return *this;
}

void Ren::Texture2D::Init(const void *data, int size, const Texture2DParams &p,
                          eTexLoadStatus *load_status, ILog *log) {
    assert(IsMainThread());
    if (!data) {
        Texture2DParams _p = p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RawRGBA8888;
        _p.filter = eTexFilter::NoFilter;
        InitFromRAWData(p.fallback_color, _p, log);
        // mark it as not ready
        ready_ = false;
        if (load_status) {
            (*load_status) = eTexLoadStatus::TexCreatedDefault;
        }
    } else {
        if (name_.EndsWith(".tga_rgbe") != 0 || name_.EndsWith(".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, p, log);
        } else if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, size, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".ktx") != 0) {
            InitFromKTXFile(data, size, p, log);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, p, log);
        } else {
            InitFromRAWData(data, p, log);
        }
        ready_ = true;
        if (load_status) {
            (*load_status) = eTexLoadStatus::TexCreatedFromData;
        }
    }
}

void Ren::Texture2D::Init(const void *data[6], const int size[6],
                          const Texture2DParams &p, eTexLoadStatus *load_status,
                          ILog *log) {
    assert(IsMainThread());
    if (!data) {
        const void *_data[6] = {p.fallback_color, p.fallback_color, p.fallback_color,
                                p.fallback_color, p.fallback_color, p.fallback_color};
        Texture2DParams _p = p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RawRGBA8888;
        _p.filter = eTexFilter::NoFilter;
        InitFromRAWData(_data, _p, log);
        // mark it as not ready
        ready_ = false;
        cubemap_ready_ = 0;
        if (load_status) {
            (*load_status) = eTexLoadStatus::TexCreatedDefault;
        }
    } else {
        if (name_.EndsWith(".tga_rgbe") != 0 || name_.EndsWith(".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, p, log);
        } else if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, p, log);
        } else if (name_.EndsWith(".png") != 0 || name_.EndsWith(".PNG") != 0) {
            InitFromPNGFile(data, size, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, size, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, size, p, log);
        } else {
            InitFromRAWData(data, p, log);
        }

        ready_ = (cubemap_ready_ & (1u << 0u)) == 1;
        for (unsigned i = 1; i < 6; i++) {
            ready_ = ready_ && ((cubemap_ready_ & (1u << i)) == 1);
        }
        if (load_status) {
            (*load_status) = eTexLoadStatus::TexCreatedFromData;
        }
    }
}

void Ren::Texture2D::Free() {
    if (params_.format != eTexFormat::Undefined && !(params_.flags & TexNoOwnership)) {
        assert(IsMainThread());
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
        tex_id_ = 0;
    }
}

void Ren::Texture2D::InitFromRAWData(const void *data, const Texture2DParams &p,
                                     ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);
    tex_id_ = tex_id;

    params_ = p;

    const auto format = (GLenum)GLFormatFromTexFormat(p.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(p.format),
               type = (GLenum)GLTypeFromTexFormat(p.format);

    auto mip_count = (GLsizei)CalcMipCount(p.w, p.h, 1, p.filter);

    if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
        // allocate all mip levels
        ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count, internal_format,
                                    (GLsizei)p.w, (GLsizei)p.h);
        // update first level
        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, 0, 0, 0, p.w, p.h, format,
                                     type, data);
    }

    ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                 AnisotropyLevel);

    ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER,
                                 g_gl_min_filter[(size_t)p.filter]);
    ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER,
                                 g_gl_mag_filter[(size_t)p.filter]);

    ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S,
                                 g_gl_wrap_mode[(size_t)p.repeat]);
    ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T,
                                 g_gl_wrap_mode[(size_t)p.repeat]);

    if (mip_count > 1 &&
        (p.filter == eTexFilter::Trilinear || p.filter == eTexFilter::Bilinear)) {
        ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_2D, tex_id);
    }

    CheckError("create texture", log);
}

void Ren::Texture2D::InitFromTGAFile(const void *data, const Texture2DParams &p,
                                     ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    std::unique_ptr<uint8_t[]> image_data = ReadTGAFile(data, w, h, format);

    Texture2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(image_data.get(), _p, log);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data, const Texture2DParams &p,
                                          ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    std::unique_ptr<uint8_t[]> image_data = ReadTGAFile(data, w, h, format);

    std::unique_ptr<uint16_t[]> fp_data = ConvertRGBE_to_RGB16F(image_data.get(), w, h);

    Texture2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = eTexFormat::RawRGB16F;

    InitFromRAWData(fp_data.get(), _p, log);
}

void Ren::Texture2D::InitFromDDSFile(const void *data, int size, const Texture2DParams &p,
                                     ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);
    tex_id_ = tex_id;

    params_ = p;
    params_.format = eTexFormat::Compressed;

    DDSHeader header;
    memcpy(&header, data, sizeof(DDSHeader));

    GLenum internal_format;
    int block_size;

    params_.w = (int)header.dwWidth;
    params_.h = (int)header.dwHeight;

    const int px_format = int(header.sPixelFormat.dwFourCC >> 24u) - '0';
    switch (px_format) {
    case 1:
        internal_format = (p.flags & TexSRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
                                              : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        block_size = 8;
        break;
    case 3:
        internal_format = (p.flags & TexSRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
                                              : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        block_size = 16;
        break;
    case 5:
        internal_format = (p.flags & TexSRGB) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                                              : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        block_size = 16;
        break;
    default:
        log->Error("Unknow DDS pixel format %i", px_format);
        return;
    }

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, (GLsizei)header.dwMipMapCount,
                                internal_format, (GLsizei)params_.w, (GLsizei)params_.h);

    int w = params_.w, h = params_.h;
    int bytes_left = size - (int)sizeof(DDSHeader);
    const uint8_t *p_data = (uint8_t *)data + sizeof(DDSHeader);

    for (uint32_t i = 0; i < header.dwMipMapCount; i++) {
        const int len = ((w + 3) / 4) * ((h + 3) / 4) * block_size;
        if (len > bytes_left) {
            log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left,
                       len);
            return;
        }

        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, (GLint)i, 0, 0, w,
                                               h, internal_format, len, p_data);

        p_data += len;
        bytes_left -= len;
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }

    SetFilter(p.filter, p.repeat, p.lod_bias);
}

void Ren::Texture2D::InitFromPNGFile(const void *data, int size, const Texture2DParams &p,
                                     ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);
    tex_id_ = tex_id;

    params_ = p;

    const unsigned res = SOIL_load_OGL_texture_from_memory(
        (unsigned char *)data, size, SOIL_LOAD_AUTO, tex_id,
        SOIL_FLAG_INVERT_Y /*| SOIL_FLAG_GL_MIPMAPS*/);
    assert(res == tex_id);

    GLint w, h;
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

    // generate mip maps manually
    if (params_.flags & (eTexFlags::TexMIPMin | eTexFlags::TexMIPMax)) {
        int width, height, channels;
        uint8_t *img_data = SOIL_load_image_from_memory((unsigned char *)data, size,
                                                        &width, &height, &channels, 0);

        std::unique_ptr<uint8_t[]> mipmaps[16];
        int widths[16] = {}, heights[16] = {};

        mipmaps[0].reset(new uint8_t[w * h * channels]);
        widths[0] = w;
        heights[0] = h;

        for (int y = 0; y < h; y++) {
            // flip y
            memcpy(&mipmaps[0][y * w * channels], &img_data[(h - y - 1) * w * channels],
                   w * channels);
        }

        // select mip operation
        const Ren::eMipOp op = (params_.flags & eTexFlags::TexMIPMin)
                                   ? Ren::eMipOp::MinBilinear
                                   : Ren::eMipOp::MaxBilinear;
        // use it for all channels
        const Ren::eMipOp ops[4] = {op, op, op, op};
        const int mip_count = InitMipMaps(mipmaps, widths, heights, 4, ops);

        for (int lod = 1; lod < mip_count; lod++) {
            glTexImage2D(GL_TEXTURE_2D, lod, (channels == 3 ? GL_RGB : GL_RGBA),
                         widths[lod], heights[lod], 0, (channels == 3 ? GL_RGB : GL_RGBA),
                         GL_UNSIGNED_BYTE, &mipmaps[lod][0]);
        }

        SOIL_free_image_data(img_data);
    }

    params_.w = (int)w;
    params_.h = (int)h;

    SetFilter(p.filter, p.repeat, p.lod_bias);
}

void Ren::Texture2D::InitFromKTXFile(const void *data, int size, const Texture2DParams &p,
                                     ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex_id);

    tex_id_ = tex_id;
    params_ = p;
    params_.format = eTexFormat::Compressed;

    KTXHeader header;
    memcpy(&header, data, sizeof(KTXHeader));

    auto internal_format = (GLenum)header.gl_internal_format;

    if ((p.flags & TexSRGB) && internal_format >= GL_COMPRESSED_RGBA_ASTC_4x4_KHR &&
        internal_format <= GL_COMPRESSED_RGBA_ASTC_12x12_KHR) {
        internal_format = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR +
                          (internal_format - GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
    }

    int w = (int)header.pixel_width;
    int h = (int)header.pixel_height;

    params_.w = w;
    params_.h = h;

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id,
                                (GLsizei)header.mipmap_levels_count, internal_format,
                                (GLsizei)w, (GLsizei)h);

    const auto *_data = (const uint8_t *)data;
    int data_offset = sizeof(KTXHeader);

    for (int i = 0; i < (int)header.mipmap_levels_count; i++) {
        if (data_offset + (int)sizeof(uint32_t) > size) {
            log->Error("Insufficient data length, bytes left %i, expected %i",
                       size - data_offset, sizeof(uint32_t));
            break;
        }

        uint32_t img_size;
        memcpy(&img_size, &_data[data_offset], sizeof(uint32_t));
        if (data_offset + (int)img_size > size) {
            log->Error("Insufficient data length, bytes left %i, expected %i",
                       size - data_offset, img_size);
            break;
        }

        data_offset += sizeof(uint32_t);

        ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, i, 0, 0, w, h,
                                               internal_format, (GLsizei)img_size,
                                               &_data[data_offset]);
        data_offset += img_size;

        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);

        const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
        data_offset += pad;
    }

    SetFilter(p.filter, p.repeat, p.lod_bias);
}

void Ren::Texture2D::InitFromRAWData(const void *data[6], const Texture2DParams &p,
                                     ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);

    tex_id_ = tex_id;
    params_ = p;

    const auto format = (GLenum)GLFormatFromTexFormat(params_.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(params_.format),
               type = (GLenum)GLTypeFromTexFormat(params_.format);

    const int w = p.w, h = p.h;
    const eTexFilter f = params_.filter;

    auto mip_count = (GLsizei)CalcMipCount(w, h, 1, f);

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_CUBE_MAP, tex_id, mip_count, internal_format,
                                w, h);

    for (unsigned i = 0; i < 6; i++) {
        if (!data[i]) {
            continue;
        } else {
            cubemap_ready_ |= (1u << i);
        }

        if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
            ren_glTextureSubImage3D_Comp(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex_id, 0, 0,
                                         0, i, w, h, 1, format, type, data[i]);
        }
    }

    if (f == eTexFilter::NoFilter) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     GL_NEAREST);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     GL_NEAREST);
    } else if (f == eTexFilter::Bilinear) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     (cubemap_ready_ == 0x3F) ? GL_LINEAR_MIPMAP_NEAREST
                                                              : GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     GL_LINEAR);
    } else if (f == eTexFilter::Trilinear) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     (cubemap_ready_ == 0x3F) ? GL_LINEAR_MIPMAP_LINEAR
                                                              : GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     GL_LINEAR);
    } else if (f == eTexFilter::BilinearNoMipmap) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     GL_LINEAR);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     GL_LINEAR);
    }

    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_S,
                                 GL_CLAMP_TO_EDGE);
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_T,
                                 GL_CLAMP_TO_EDGE);
#if !defined(GL_ES_VERSION_2_0) && !defined(__EMSCRIPTEN__)
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_R,
                                 GL_CLAMP_TO_EDGE);
#endif

    if ((f == eTexFilter::Trilinear || f == eTexFilter::Bilinear) &&
        (cubemap_ready_ == 0x3F)) {
        ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_CUBE_MAP, tex_id);
    }
}

void Ren::Texture2D::InitFromTGAFile(const void *data[6], const Texture2DParams &p,
                                     ILog *log) {
    std::unique_ptr<uint8_t[]> image_data[6];
    const void *_image_data[6] = {};
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    for (int i = 0; i < 6; i++) {
        if (data[i]) {
            image_data[i] = ReadTGAFile(data[i], w, h, format);
            _image_data[i] = image_data[i].get();
        }
    }

    Texture2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(_image_data, _p, log);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data[6], const Texture2DParams &p,
                                          ILog *log) {
    std::unique_ptr<uint16_t[]> image_data[6];
    const void *_image_data[6] = {};
    int w = p.w, h = p.h;
    for (int i = 0; i < 6; i++) {
        if (data[i]) {
            image_data[i] = ConvertRGBE_to_RGB16F((const uint8_t *)data[i], w, h);
            _image_data[i] = image_data[i].get();
        }
    }

    Texture2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = Ren::eTexFormat::RawRGB16F;

    InitFromRAWData(_image_data, _p, log);
}

void Ren::Texture2D::InitFromPNGFile(const void *data[6], const int size[6],
                                     const Texture2DParams &p, ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    params_ = p;

    unsigned int handle = SOIL_load_OGL_cubemap_from_memory(
        (const unsigned char *)data[0], size[0], (const unsigned char *)data[1], size[1],
        (const unsigned char *)data[2], size[2], (const unsigned char *)data[3], size[3],
        (const unsigned char *)data[4], size[4], (const unsigned char *)data[5], size[5],
        0, tex_id, SOIL_FLAG_INVERT_Y);
    assert(handle == tex_id);

    GLint w, h;
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_HEIGHT, &h);

    params_.w = (int)w;
    params_.h = (int)h;
    params_.cube = 1;

    SetFilter(p.filter, p.repeat, p.lod_bias);
}

void Ren::Texture2D::InitFromDDSFile(const void *data[6], const int size[6],
                                     const Texture2DParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    tex_id_ = tex_id;
    params_ = p;

    for (int i = 0; i < 6; i++) {
        DDSHeader header = {};
        memcpy(&header, data[i], sizeof(DDSHeader));

        const uint8_t *pdata = (uint8_t *)data[i] + sizeof(DDSHeader);
        int data_len = size[i] - int(sizeof(DDSHeader));

        for (uint32_t j = 0; j < header.dwMipMapCount; j++) {
            const int width = std::max(int(header.dwWidth >> j), 1),
                      height = std::max(int(header.dwHeight >> j), 1);

            GLenum format = 0;
            int block_size = 0;

            switch ((header.sPixelFormat.dwFourCC >> 24u) - '0') {
            case 1:
                format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                block_size = 8;
                break;
            case 3:
                format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                block_size = 16;
                break;
            case 5:
                format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                block_size = 16;
                break;
            default:
                log->Error("Unknown DDS format %i",
                           int((header.sPixelFormat.dwFourCC >> 24u) - '0'));
                break;
            }

            const int image_len = ((width + 3) / 4) * ((height + 3) / 4) * block_size;
            if (image_len > data_len) {
                log->Error("Insufficient data length, bytes left %i, expected %i",
                           data_len, image_len);
                break;
            }

            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, j, format, width,
                                   height, 0, image_len, pdata);

            pdata += image_len;
            data_len -= image_len;
        }
    }

    params_.cube = 1;

    SetFilter(p.filter, p.repeat, p.lod_bias);
}

void Ren::Texture2D::InitFromKTXFile(const void *data[6], const int size[6],
                                     const Texture2DParams &p, ILog *log) {
    (void)size;

    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);

    tex_id_ = tex_id;
    params_ = p;
    params_.format = eTexFormat::Compressed;

    KTXHeader first_header;
    memcpy(&first_header, data[0], sizeof(KTXHeader));

    const int w = (int)first_header.pixel_width, h = (int)first_header.pixel_height;

    params_.w = w;
    params_.h = h;
    params_.cube = true;

    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    for (int j = 0; j < 6; j++) {
        const auto *_data = (const uint8_t *)data[j];

#ifndef NDEBUG
        KTXHeader this_header;
        memcpy(&this_header, data[j], sizeof(KTXHeader));

        // make sure all images have same properties
        if (this_header.pixel_width != first_header.pixel_width) {
            log->Error("Image width mismatch %i, expected %i",
                       (int)this_header.pixel_width, (int)first_header.pixel_width);
            continue;
        }
        if (this_header.pixel_height != first_header.pixel_height) {
            log->Error("Image height mismatch %i, expected %i",
                       (int)this_header.pixel_height, (int)first_header.pixel_height);
            continue;
        }
        if (this_header.gl_internal_format != first_header.gl_internal_format) {
            log->Error("Internal format mismatch %i, expected %i",
                       (int)this_header.gl_internal_format,
                       (int)first_header.gl_internal_format);
            continue;
        }
#endif
        int data_offset = sizeof(KTXHeader);

        int _w = w, _h = h;

        for (int i = 0; i < (int)first_header.mipmap_levels_count; i++) {
            uint32_t img_size;
            memcpy(&img_size, &_data[data_offset], sizeof(uint32_t));
            data_offset += sizeof(uint32_t);
            glCompressedTexImage2D((GLenum)(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j), i,
                                   (GLenum)first_header.gl_internal_format, _w, _h, 0,
                                   (GLsizei)img_size, &_data[data_offset]);
            data_offset += img_size;

            _w = std::max(_w / 2, 1);
            _h = std::max(_h / 2, 1);

            const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
            data_offset += pad;
        }
    }

    SetFilter(p.filter, p.repeat, p.lod_bias);
}

void Ren::Texture2D::SetFilter(eTexFilter f, eTexRepeat r, float lod_bias) {
    auto tex_id = (GLuint)tex_id_;

    if (!params_.cube) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[(size_t)f]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[(size_t)f]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S,
                                     g_gl_wrap_mode[(size_t)r]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T,
                                     g_gl_wrap_mode[(size_t)r]);

#ifndef __ANDROID__
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_LOD_BIAS,
                                     (params_.flags & eTexFlags::TexNoBias) ? 0.0f
                                                                            : lod_bias);
#endif

        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                     AnisotropyLevel);

        if (params_.format != eTexFormat::Compressed &&
            (f == eTexFilter::Trilinear || f == eTexFilter::Bilinear)) {
            if (!(params_.flags & (eTexFlags::TexMIPMin | eTexFlags::TexMIPMax))) {
                ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_2D, tex_id);
            }
        }
    } else {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[(size_t)f]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[(size_t)f]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_S,
                                     g_gl_wrap_mode[(size_t)r]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_T,
                                     g_gl_wrap_mode[(size_t)r]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_R,
                                     g_gl_wrap_mode[(size_t)r]);

        if (params_.format != eTexFormat::Compressed &&
            (f == eTexFilter::Trilinear || f == eTexFilter::Bilinear)) {
            ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_CUBE_MAP, tex_id);
        }
    }
}

void Ren::Texture2D::ReadTextureData(eTexFormat format, void *out_data) const {
#if defined(__ANDROID__)
#else
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id_);

    if (format == eTexFormat::RawRGBA8888) {
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out_data);
    }
#endif
}

uint32_t Ren::GLFormatFromTexFormat(eTexFormat format) {
    return g_gl_formats[(size_t)format];
}

uint32_t Ren::GLInternalFormatFromTexFormat(eTexFormat format) {
    return g_gl_internal_formats[(size_t)format];
}

uint32_t Ren::GLTypeFromTexFormat(eTexFormat format) {
    return g_gl_types[(size_t)format];
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
