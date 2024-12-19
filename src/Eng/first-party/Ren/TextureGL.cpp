#include "TextureGL.h"

#include <memory>
#undef min
#undef max

#include "Config.h"
#include "Context.h"
#include "GL.h"
#include "Utils.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#ifndef NDEBUG
// #define TEX_VERBOSE_LOGGING
#endif

namespace Ren {
const uint32_t g_gl_formats[] = {
    0xffffffff,         // Undefined
    GL_RGB,             // RGB8
    GL_RGBA,            // RGBA8
    GL_RGBA,            // RawSignedRGBA8888
    0xffffffff,         // BGRA8
    GL_RED,             // R32F
    GL_RED,             // R16F
    GL_RED,             // R8
    GL_RED_INTEGER,     // R32UI
    GL_RG,              // RG8
    GL_RGB,             // RGB32F
    GL_RGBA,            // RGBA32F
    GL_RGBA,            // RGBA32UI
    0xffffffff,         // RGBE8
    GL_RGB,             // RGB16F
    GL_RGBA,            // RGBA16F
    GL_RG,              // RG16_snorm
    GL_RG,              // RG16
    GL_RG,              // RG16F
    GL_RG,              // RG32F
    GL_RG_INTEGER,      // RG32UI
    GL_RGBA,            // RGB10_A2
    GL_RGB,             // RG11F_B10F
    GL_RGB,             // RGB9_E5
    GL_DEPTH_COMPONENT, // D16
    GL_DEPTH_STENCIL,   // D24_S8
    GL_DEPTH_STENCIL,   // D32_S8
    GL_DEPTH_COMPONENT, // D32
    0xffffffff,         // BC1
    0xffffffff,         // BC2
    0xffffffff,         // BC3
    0xffffffff,         // BC4
    0xffffffff,         // BC5
    0xffffffff          // ASTC
};
static_assert(std::size(g_gl_formats) == size_t(eTexFormat::_Count), "!");

const uint32_t g_gl_internal_formats[] = {
    0xffffffff,           // Undefined
    GL_RGB8,              // RGB8
    GL_RGBA8,             // RGBA8
    GL_RGBA8_SNORM,       // RawSignedRGBA8888
    0xffffffff,           // BGRA8
    GL_R32F,              // R32F
    GL_R16F,              // R16F
    GL_R8,                // R8
    GL_R32UI,             // R32UI
    GL_RG8,               // RG8
    GL_RGB32F,            // RGB32F
    GL_RGBA32F,           // RGBA32F
    GL_RGBA32UI,          // RGBA32UI
    0xffffffff,           // RGBE8
    GL_RGB16F,            // RGB16F
    GL_RGBA16F,           // RGBA16F
    GL_RG16_SNORM_EXT,    // RG16_snorm
    GL_RG16_EXT,          // RG16
    GL_RG16F,             // RG16F
    GL_RG32F,             // RG32F
    GL_RG32UI,            // RG32UI
    GL_RGB10_A2,          // RGB10_A2
    GL_R11F_G11F_B10F,    // RG11F_B10F
    GL_RGB9_E5,           // RGB9_E5
    GL_DEPTH_COMPONENT16, // D16
    GL_DEPTH24_STENCIL8,  // D24_S8
    GL_DEPTH32F_STENCIL8, // D32_S8
#ifndef __ANDROID__
    GL_DEPTH_COMPONENT32, // D32
#endif
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,  // BC1
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,  // BC2
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,  // BC3
    GL_COMPRESSED_RED_RGTC1_EXT,       // BC4
    GL_COMPRESSED_RED_GREEN_RGTC2_EXT, // BC5
    0xffffffff                         // ASTC
};
static_assert(std::size(g_gl_internal_formats) == size_t(eTexFormat::_Count), "!");

const uint32_t g_gl_types[] = {
    0xffffffff,                        // Undefined
    GL_UNSIGNED_BYTE,                  // RGB8
    GL_UNSIGNED_BYTE,                  // RGBA8
    GL_BYTE,                           // RawSignedRGBA8888
    GL_UNSIGNED_BYTE,                  // BGRA8
    GL_FLOAT,                          // R32F
    GL_HALF_FLOAT,                     // R16F
    GL_UNSIGNED_BYTE,                  // R8
    GL_UNSIGNED_INT,                   // R32UI
    GL_UNSIGNED_BYTE,                  // RG8
    GL_FLOAT,                          // RGB32F
    GL_FLOAT,                          // RGBA32F
    GL_UNSIGNED_INT,                   // RGBA32UI
    0xffffffff,                        // RGBE8
    GL_HALF_FLOAT,                     // RGB16F
    GL_HALF_FLOAT,                     // RGBA16F
    GL_SHORT,                          // RG16_snorm
    GL_UNSIGNED_SHORT,                 // RG16
    GL_HALF_FLOAT,                     // RG16F
    GL_FLOAT,                          // RG32F
    GL_UNSIGNED_INT,                   // RG32UI
    GL_UNSIGNED_INT_2_10_10_10_REV,    // RGB10_A2
    GL_FLOAT,                          // RG11F_B10F
    GL_UNSIGNED_INT_5_9_9_9_REV,       // RGB9_E5
    GL_UNSIGNED_SHORT,                 // D16
    GL_FLOAT_32_UNSIGNED_INT_24_8_REV, // D24_S8
    GL_FLOAT,                          // D32_S8
    GL_UNSIGNED_INT,                   // D32
    0xffffffff,                        // BC1
    0xffffffff,                        // BC2
    0xffffffff,                        // BC3
    0xffffffff,                        // BC4
    0xffffffff,                        // BC5
    0xffffffff                         // ASTC
};
static_assert(std::size(g_gl_types) == size_t(eTexFormat::_Count), "!");

const uint32_t g_gl_compare_func[] = {
    0xffffffff,  // None
    GL_LEQUAL,   // LEqual
    GL_GEQUAL,   // GEqual
    GL_LESS,     // Less
    GL_GREATER,  // Greater
    GL_EQUAL,    // Equal
    GL_NOTEQUAL, // NotEqual
    GL_ALWAYS,   // Always
    GL_NEVER,    // Never
};
static_assert(std::size(g_gl_compare_func) == size_t(eTexCompare::_Count), "!");

uint32_t TextureHandleCounter = 0;

GLenum ToSRGBFormat(const GLenum internal_format) {
    switch (internal_format) {
    case GL_RGB8:
        return GL_SRGB8;
    case GL_RGBA8:
        return GL_SRGB8_ALPHA8;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR;
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR;
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR;
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR;
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR;
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR;
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR;
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR;
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR;
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR;
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR;
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR;
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR;
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR;
    default:
        assert(false && "Unsupported format!");
    }

    return 0xffffffff;
}

extern const uint32_t g_gl_min_filter[];
extern const uint32_t g_gl_mag_filter[];
extern const uint32_t g_gl_wrap_mode[];

extern const float AnisotropyLevel;
} // namespace Ren

static_assert(sizeof(GLsync) == sizeof(void *), "!");

Ren::Texture2D::Texture2D(std::string_view name, ApiContext *api_ctx, const Tex2DParams &p, MemAllocators *, ILog *log)
    : name_(name) {
    Init(p, nullptr, log);
}

Ren::Texture2D::Texture2D(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data, const Tex2DParams &p,
                          MemAllocators *, eTexLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, p, nullptr, load_status, log);
}

Ren::Texture2D::Texture2D(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data[6], const Tex2DParams &p,
                          MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, p, nullptr, load_status, log);
}

Ren::Texture2D::~Texture2D() { Free(); }

Ren::Texture2D &Ren::Texture2D::operator=(Texture2D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    handle_ = std::exchange(rhs.handle_, {});
    initialized_mips_ = std::exchange(rhs.initialized_mips_, 0);
    params = std::exchange(rhs.params, {});
    ready_ = std::exchange(rhs.ready_, false);
    name_ = std::move(rhs.name_);

    return (*this);
}

uint64_t Ren::Texture2D::GetBindlessHandle() const { return glGetTextureHandleARB(GLuint(handle_.id)); }

void Ren::Texture2D::Init(const Tex2DParams &p, MemAllocators *, ILog *log) {
    InitFromRAWData(nullptr, 0, p, log);
    ready_ = true;
}

void Ren::Texture2D::Init(const TexHandle &handle, const Tex2DParams &_params, MemAllocation &&alloc, ILog *log) {
    handle_ = handle;
    params = _params;

    SetSampling(params.sampling);
    ready_ = true;
}

void Ren::Texture2D::Init(Span<const uint8_t> data, const Tex2DParams &p, MemAllocators *mem_allocs,
                          eTexLoadStatus *load_status, ILog *log) {
    if (data.empty()) {
        auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, 4};
        { // Update staging buffer
            uint8_t *stage_data = sbuf.Map();
            memcpy(stage_data, p.fallback_color, 4);
            sbuf.Unmap();
        }

        Tex2DParams _p = p;
        _p.w = _p.h = 1;
        _p.mip_count = 1;
        _p.format = eTexFormat::RGBA8;
        InitFromRAWData(&sbuf, 0, _p, log);
        // mark it as not ready
        ready_ = false;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, p, log);
        } else {
            auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, uint32_t(data.size())};
            { // Update staging buffer
                uint8_t *stage_data = sbuf.Map();
                memcpy(stage_data, data.data(), data.size());
                sbuf.Unmap();
            }
            InitFromRAWData(&sbuf, 0, p, log);
        }
        ready_ = true;
        (*load_status) = eTexLoadStatus::CreatedFromData;
    }
}

void Ren::Texture2D::Init(Span<const uint8_t> data[6], const Tex2DParams &p, MemAllocators *mem_allocs,
                          eTexLoadStatus *load_status, ILog *log) {
    if (!data) {
        auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, 4};
        { // Update staging buffer
            uint8_t *stage_data = sbuf.Map();
            memcpy(stage_data, p.fallback_color, 4);
            sbuf.Unmap();
        }

        int data_off[6] = {};

        Tex2DParams _p = p;
        _p.w = _p.h = 1;
        _p.format = eTexFormat::RGBA8;
        InitFromRAWData(sbuf, data_off, _p, log);
        // mark it as not ready
        ready_ = false;
        (*load_status) = eTexLoadStatus::CreatedDefault;
    } else {
        if (name_.EndsWith(".tga") != 0 || name_.EndsWith(".TGA") != 0) {
            InitFromTGAFile(data, p, log);
        } else if (name_.EndsWith(".ktx") != 0 || name_.EndsWith(".KTX") != 0) {
            InitFromKTXFile(data, p, log);
        } else if (name_.EndsWith(".dds") != 0 || name_.EndsWith(".DDS") != 0) {
            InitFromDDSFile(data, p, log);
        } else {
            auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload,
                               uint32_t(data[0].size() + data[1].size() + data[2].size() + data[3].size() +
                                        data[4].size() + data[5].size())};
            int data_off[6];
            { // Update staging buffer
                uint8_t *stage_data = sbuf.Map();
                uint32_t stage_off = 0;

                for (int i = 0; i < 6; i++) {
                    if (!data[i].empty()) {
                        memcpy(&stage_data[stage_off], data[i].data(), data[i].size());
                        data_off[i] = int(stage_off);
                        stage_off += uint32_t(data[i].size());
                    } else {
                        data_off[i] = -1;
                    }
                }
                sbuf.Unmap();
            }
            InitFromRAWData(sbuf, data_off, p, log);
        }

        ready_ = true;
        (*load_status) = eTexLoadStatus::CreatedFromData;
    }
}

void Ren::Texture2D::Free() {
    if (params.format != eTexFormat::Undefined && !bool(params.flags & eTexFlagBits::NoOwnership)) {
        auto tex_id = GLuint(handle_.id);
        glDeleteTextures(1, &tex_id);
        for (uint32_t view : handle_.views) {
            if (view != tex_id) {
                auto tex_id = GLuint(view);
                glDeleteTextures(1, &tex_id);
            }
        }
        handle_ = {};
        params.format = eTexFormat::Undefined;
    }
}

void Ren::Texture2D::Realloc(const int w, const int h, int mip_count, const int samples, const eTexFormat format,
                             const eTexBlock block, const bool is_srgb, CommandBuffer cmd_buf,
                             MemAllocators *mem_allocs, ILog *log) {
    GLuint tex_id;
    glCreateTextures(samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    const GLuint internal_format = GLInternalFormatFromTexFormat(format, is_srgb);

    mip_count = std::min(mip_count, CalcMipCount(w, h, 1, eTexFilter::Trilinear));

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, GLsizei(mip_count), internal_format, GLsizei(w), GLsizei(h));
#ifdef TEX_VERBOSE_LOGGING
    if (params_.format != eTexFormat::Undefined) {
        log->Info("Realloc %s, %ix%i (%i mips) -> %ix%i (%i mips)", name_.c_str(), int(params_.w), int(params_.h),
                  int(params_.mip_count), w, h, mip_count);
    } else {
        log->Info("Alloc %s %ix%i (%i mips)", name_.c_str(), w, h, mip_count);
    }
#endif

    const TexHandle new_handle = {tex_id, TextureHandleCounter++};
    uint16_t new_initialized_mips = 0;

    // copy data from old texture
    if (params.format == format) {
        int src_mip = 0, dst_mip = 0;
        while (std::max(params.w >> src_mip, 1) != std::max(w >> dst_mip, 1) ||
               std::max(params.h >> src_mip, 1) != std::max(h >> dst_mip, 1)) {
            if (std::max(params.w >> src_mip, 1) > std::max(w >> dst_mip, 1) ||
                std::max(params.h >> src_mip, 1) > std::max(h >> dst_mip, 1)) {
                ++src_mip;
            } else {
                ++dst_mip;
            }
        }

        for (; src_mip < int(params.mip_count) && dst_mip < mip_count; ++src_mip, ++dst_mip) {
            if (initialized_mips_ & (1u << src_mip)) {
                glCopyImageSubData(GLuint(handle_.id), GL_TEXTURE_2D, GLint(src_mip), 0, 0, 0, GLuint(new_handle.id),
                                   GL_TEXTURE_2D, GLint(dst_mip), 0, 0, 0, GLsizei(std::max(params.w >> src_mip, 1)),
                                   GLsizei(std::max(params.h >> src_mip, 1)), 1);
#ifdef TEX_VERBOSE_LOGGING
                log->Info("Copying data mip %i [old] -> mip %i [new]", src_mip, dst_mip);
#endif

                new_initialized_mips |= (1u << dst_mip);
            }
        }
    }
    Free();

    handle_ = new_handle;
    params.w = w;
    params.h = h;
    if (is_srgb) {
        params.flags |= eTexFlagBits::SRGB;
    } else {
        params.flags &= ~eTexFlagBits::SRGB;
    }
    params.mip_count = mip_count;
    params.samples = samples;
    params.format = format;
    params.block = block;
    initialized_mips_ = new_initialized_mips;

    if (uint32_t(params.flags & eTexFlagBits::ExtendedViews) != 0) {
        // create additional image views
        for (int j = 0; j < mip_count; ++j) {
            GLuint tex_view;
            glGenTextures(1, &tex_view);
            glTextureView(tex_view, GL_TEXTURE_2D, tex_id, internal_format, j, 1, 0, 1);
#ifdef ENABLE_GPU_DEBUG
            glObjectLabel(GL_TEXTURE, tex_view, -1, name_.c_str());
#endif
            handle_.views.push_back(tex_view);
        }
    }
}

void Ren::Texture2D::InitFromRAWData(const Buffer *sbuf, int data_off, const Tex2DParams &p, ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(p.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    handle_ = {tex_id, TextureHandleCounter++};

    params = p;
    initialized_mips_ = 0;

    const auto format = (GLenum)GLFormatFromTexFormat(p.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(p.format, bool(p.flags & eTexFlagBits::SRGB)),
               type = (GLenum)GLTypeFromTexFormat(p.format);

    auto mip_count = GLsizei(p.mip_count);
    if (!mip_count) {
        mip_count = GLsizei(CalcMipCount(p.w, p.h, 1, p.sampling.filter));
    }

    if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
        if (p.samples > 1) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex_id);
            glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, GLsizei(p.samples), internal_format, GLsizei(p.w),
                                      GLsizei(p.h), GL_TRUE);
            initialized_mips_ |= (1u << 0);
        } else {
            // allocate all mip levels
            ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count, internal_format, GLsizei(p.w), GLsizei(p.h));
            if (sbuf) {
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf->id());

                // update first level
                ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, 0, 0, 0, p.w, p.h, format, type,
                                             reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
                initialized_mips_ |= (1u << 0);

                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }
        }
    }

    if (IsDepthStencilFormat(p.format)) {
        // create additional 'depth-only' image view (for compatibility with VK)
        handle_.views.push_back(tex_id);
    }

    if (uint32_t(params.flags & eTexFlagBits::ExtendedViews) != 0) {
        // create additional image views
        for (int j = 0; j < mip_count; ++j) {
            GLuint tex_view;
            glGenTextures(1, &tex_view);
            glTextureView(tex_view, GL_TEXTURE_2D, tex_id, internal_format, j, 1, 0, 1);
#ifdef ENABLE_GPU_DEBUG
            glObjectLabel(GL_TEXTURE, tex_view, -1, name_.c_str());
#endif
            handle_.views.push_back(tex_view);
        }
    }

    if (p.samples == 1) {
        ApplySampling(p.sampling, log);
    }

    CheckError("create texture", log);
}

void Ren::Texture2D::InitFromTGAFile(Span<const uint8_t> data, const Tex2DParams &p, ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;
    uint32_t img_size = 0;
    const bool res1 = ReadTGAFile(data, w, h, format, nullptr, img_size);
    if (!res1) {
        return;
    }

    auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, img_size};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        const bool res2 = ReadTGAFile(data, w, h, format, stage_data, img_size);
        assert(res2);
        sbuf.Unmap();
    }

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(&sbuf, 0, _p, log);
}

void Ren::Texture2D::InitFromDDSFile(Span<const uint8_t> data, const Tex2DParams &p, ILog *log) {
    Free();

    int bytes_left = int(data.size());
    const uint8_t *p_data = data.data();

    DDSHeader header;
    memcpy(&header, data.data(), sizeof(DDSHeader));
    p_data += sizeof(DDSHeader);
    bytes_left -= sizeof(DDSHeader);

    Tex2DParams _p = p;
    ParseDDSHeader(header, &_p);

    if (header.sPixelFormat.dwFourCC ==
        ((unsigned('D') << 0u) | (unsigned('X') << 8u) | (unsigned('1') << 16u) | (unsigned('0') << 24u))) {
        DDS_HEADER_DXT10 dx10_header = {};
        memcpy(&dx10_header, data.data() + sizeof(DDSHeader), sizeof(DDS_HEADER_DXT10));
        _p.format = TexFormatFromDXGIFormat(dx10_header.dxgiFormat);

        p_data += sizeof(DDS_HEADER_DXT10);
        bytes_left -= sizeof(DDS_HEADER_DXT10);
    } else if (_p.format == eTexFormat::Undefined) {
        // Try to use least significant bits of FourCC as format
        const uint8_t val = (header.sPixelFormat.dwFourCC & 0xff);
        if (val == 0x6f) {
            _p.format = eTexFormat::R16F;
        } else if (val == 0x70) {
            _p.format = eTexFormat::RG16F;
        } else if (val == 0x71) {
            _p.format = eTexFormat::RGBA16F;
        } else if (val == 0x72) {
            _p.format = eTexFormat::R32F;
        } else if (val == 0x73) {
            _p.format = eTexFormat::RG32F;
        } else if (val == 0x74) {
            _p.format = eTexFormat::RGBA32F;
        } else if (val == 0) {
            if (header.sPixelFormat.dwRGBBitCount == 8) {
                _p.format = eTexFormat::R8;
            } else if (header.sPixelFormat.dwRGBBitCount == 16) {
                _p.format = eTexFormat::RG8;
                assert(header.sPixelFormat.dwRBitMask == 0x00ff);
                assert(header.sPixelFormat.dwGBitMask == 0xff00);
            }
        }
    }

    if (_p.format == eTexFormat::Undefined) {
        log->Error("Failed to parse DDS header!");
        return;
    }

    params.usage = _p.usage;

    Realloc(_p.w, _p.h, _p.mip_count, 1, _p.format, _p.block, bool(_p.flags & eTexFlagBits::SRGB), nullptr, nullptr,
            log);

    params.flags = _p.flags;
    params.block = _p.block;
    params.sampling = _p.sampling;

    const auto format = (GLenum)GLFormatFromTexFormat(_p.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(_p.format, bool(_p.flags & eTexFlagBits::SRGB)),
               type = (GLenum)GLTypeFromTexFormat(_p.format);

    auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, uint32_t(bytes_left)};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        memcpy(stage_data, p_data, bytes_left);
        sbuf.Unmap();
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    int w = params.w, h = params.h;
    uintptr_t data_off = 0;
    for (uint32_t i = 0; i < header.dwMipMapCount; i++) {
        const int len = GetMipDataLenBytes(w, h, params.format, params.block);
        if (len > bytes_left) {
            log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
            return;
        }

        if (IsCompressedFormat(params.format)) {
            ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), GLint(i), 0, 0, w, h,
                                                   internal_format, len, reinterpret_cast<const GLvoid *>(data_off));
        } else {
            ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), 0, 0, 0, w, h, format, type,
                                         reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
        }
        initialized_mips_ |= (1u << i);

        data_off += len;
        bytes_left -= len;
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromKTXFile(Span<const uint8_t> data, const Tex2DParams &p, ILog *log) {
    KTXHeader header;
    memcpy(&header, data.data(), sizeof(KTXHeader));

    eTexBlock block;
    bool is_srgb_format;
    eTexFormat format = FormatFromGLInternalFormat(header.gl_internal_format, &block, &is_srgb_format);

    if (is_srgb_format && bool(params.flags & eTexFlagBits::SRGB)) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    Free();
    Realloc(int(header.pixel_width), int(header.pixel_height), int(header.mipmap_levels_count), 1, format, block,
            bool(p.flags & eTexFlagBits::SRGB), nullptr, nullptr, log);

    params.flags = p.flags;
    params.block = block;
    params.sampling = p.sampling;

    int w = int(params.w);
    int h = int(params.h);

    params.w = w;
    params.h = h;

    int data_offset = sizeof(KTXHeader);

    auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, uint32_t(data.size() - data_offset)};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        memcpy(stage_data, data.data(), data.size() - data_offset);
        sbuf.Unmap();
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    for (int i = 0; i < int(header.mipmap_levels_count); i++) {
        if (data_offset + int(sizeof(uint32_t)) > data.size()) {
            log->Error("Insufficient data length, bytes left %i, expected %i", int(data.size() - data_offset),
                       int(sizeof(uint32_t)));
            break;
        }

        uint32_t img_size;
        memcpy(&img_size, &data[data_offset], sizeof(uint32_t));
        if (data_offset + int(img_size) > data.size()) {
            log->Error("Insufficient data length, bytes left %i, expected %i", int(data.size() - data_offset),
                       img_size);
            break;
        }

        data_offset += sizeof(uint32_t);

        ren_glCompressedTextureSubImage2D_Comp(
            GL_TEXTURE_2D, GLuint(handle_.id), i, 0, 0, w, h,
            GLInternalFormatFromTexFormat(params.format, bool(params.flags & eTexFlagBits::SRGB)), GLsizei(img_size),
            reinterpret_cast<const GLvoid *>(uintptr_t(data_offset)));
        initialized_mips_ |= (1u << i);
        data_offset += img_size;

        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);

        const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
        data_offset += pad;
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromRAWData(const Buffer &sbuf, int data_off[6], const Tex2DParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    handle_ = {tex_id, TextureHandleCounter++};
    params = p;

    const auto format = (GLenum)GLFormatFromTexFormat(params.format),
               internal_format =
                   (GLenum)GLInternalFormatFromTexFormat(params.format, bool(p.flags & eTexFlagBits::SRGB)),
               type = (GLenum)GLTypeFromTexFormat(params.format);

    const int w = p.w, h = p.h;
    const eTexFilter f = params.sampling.filter;

    const int mip_count = CalcMipCount(w, h, 1, f);
    params.mip_count = mip_count;

    // allocate all mip levels
    ren_glTextureStorage2D_Comp(GL_TEXTURE_CUBE_MAP, tex_id, mip_count, internal_format, w, h);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    for (int i = 0; i < 6; ++i) {
        uint32_t buffer_offset = data_off[i];
        for (int j = 0; j < mip_count; ++j) {
            const int _w = (w >> j), _h = (h >> j);
            if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
                ren_glTextureSubImage3D_Comp(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex_id, j, 0, 0, i, _w, _h, 1, format,
                                             type, reinterpret_cast<const GLvoid *>(uintptr_t(buffer_offset)));
                buffer_offset += GetMipDataLenBytes(_w, _h, p.format, p.block);
            }
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    if (uint32_t(params.flags & eTexFlagBits::ExtendedViews) != 0) {
        // create additional image views
        for (int j = 0; j < mip_count; ++j) {
            for (int i = 0; i < 6; ++i) {
                GLuint tex_view;
                glGenTextures(1, &tex_view);
                glTextureView(tex_view, GL_TEXTURE_2D, tex_id, internal_format, j, 1, i, 1);
#ifdef ENABLE_GPU_DEBUG
                glObjectLabel(GL_TEXTURE, tex_view, -1, name_.c_str());
#endif
                handle_.views.push_back(tex_view);
            }
        }
    }

    params.cube = 1;

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromTGAFile(Span<const uint8_t> data[6], const Tex2DParams &p, ILog *log) {
    int w = 0, h = 0;
    eTexFormat format = eTexFormat::Undefined;

    auto sbuf = Buffer{
        "Temp Stage Buf", nullptr, eBufType::Upload,
        uint32_t(data[0].size() + data[1].size() + data[2].size() + data[3].size() + data[4].size() + data[5].size())};
    int data_off[6] = {-1, -1, -1, -1, -1, -1};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        uint32_t stage_off = 0;

        int data_off[6] = {-1, -1, -1, -1, -1, -1};

        for (int i = 0; i < 6; i++) {
            if (!data[i].empty()) {
                uint32_t data_size;
                const bool res1 = ReadTGAFile(data[i], w, h, format, nullptr, data_size);
                assert(res1);

                assert(stage_off + data_size < sbuf.size());
                const bool res2 = ReadTGAFile(data[i], w, h, format, &stage_data[stage_off], data_size);
                assert(res2);

                data_off[i] = int(stage_off);
                stage_off += data_size;
            }
        }
        sbuf.Unmap();
    }

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(sbuf, data_off, _p, log);
}

void Ren::Texture2D::InitFromDDSFile(Span<const uint8_t> data[6], const Tex2DParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    uint32_t data_off[6] = {};
    uint32_t stage_len = 0;

    GLenum first_format = 0;
    eTexBlock first_block = eTexBlock::_None;

    for (int i = 0; i < 6; ++i) {
        const DDSHeader *header = reinterpret_cast<const DDSHeader *>(data[i].data());

        GLenum format = 0;
        eTexBlock block;

        switch ((header->sPixelFormat.dwFourCC >> 24u) - '0') {
        case 1:
            format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            block = eTexBlock::_4x4;
            break;
        case 3:
            format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            block = eTexBlock::_4x4;
            break;
        case 5:
            format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            block = eTexBlock::_4x4;
            break;
        default:
            log->Error("Unknown DDS format %i", int((header->sPixelFormat.dwFourCC >> 24u) - '0'));
            return;
        }

        if (i == 0) {
            first_format = format;
            first_block = block;
        } else {
            assert(format == first_format);
            assert(block == first_block);
        }

        data_off[i] = stage_len;
        stage_len += uint32_t(data[i].size());
    }

    auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, stage_len};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        for (int i = 0; i < 6; ++i) {
            memcpy(stage_data + data_off[i], data[i].data(), data[i].size());
        }
        sbuf.Unmap();
    }

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    handle_ = {tex_id, TextureHandleCounter++};
    params = p;
    params.block = first_block;
    initialized_mips_ = 0;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    for (int i = 0; i < 6; i++) {
        const DDSHeader *header = reinterpret_cast<const DDSHeader *>(data[i].data());
        int data_offset = sizeof(DDSHeader);
        for (uint32_t j = 0; j < header->dwMipMapCount; j++) {
            const int width = std::max(int(header->dwWidth >> j), 1), height = std::max(int(header->dwHeight >> j), 1);

            const int image_len = ((width + 3) / 4) * ((height + 3) / 4) * BlockLenFromGLInternalFormat(first_format);
            if (data_off[i] + image_len > stage_len) {
                log->Error("Insufficient data length!");
                break;
            }

            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, j, first_format, width, height, 0, image_len,
                                   reinterpret_cast<const GLvoid *>(uintptr_t(data_off[i] + data_offset)));

            data_offset += image_len;
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    params.cube = 1;

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::InitFromKTXFile(Span<const uint8_t> data[6], const Tex2DParams &p, ILog *log) {
    Free();

    const auto *first_header = reinterpret_cast<const KTXHeader *>(data[0].data());

    uint32_t data_off[6] = {};
    uint32_t stage_len = 0;

    for (int i = 0; i < 6; ++i) {
        const auto *this_header = reinterpret_cast<const KTXHeader *>(data[i].data());

        // make sure all images have same properties
        if (this_header->pixel_width != first_header->pixel_width) {
            log->Error("Image width mismatch %i, expected %i", int(this_header->pixel_width),
                       int(first_header->pixel_width));
            continue;
        }
        if (this_header->pixel_height != first_header->pixel_height) {
            log->Error("Image height mismatch %i, expected %i", int(this_header->pixel_height),
                       int(first_header->pixel_height));
            continue;
        }
        if (this_header->gl_internal_format != first_header->gl_internal_format) {
            log->Error("Internal format mismatch %i, expected %i", int(this_header->gl_internal_format),
                       int(first_header->gl_internal_format));
            continue;
        }

        data_off[i] = stage_len;
        stage_len += uint32_t(data[i].size());
    }

    auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, stage_len};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        for (int i = 0; i < 6; ++i) {
            memcpy(stage_data + data_off[i], data[i].data(), data[i].size());
        }
        sbuf.Unmap();
    }

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    handle_ = {tex_id, TextureHandleCounter++};
    params = p;
    initialized_mips_ = 0;

    bool is_srgb_format;
    params.format = FormatFromGLInternalFormat(first_header->gl_internal_format, &params.block, &is_srgb_format);

    if (is_srgb_format && !bool(params.flags & eTexFlagBits::SRGB)) {
        log->Warning("Loading SRGB texture as non-SRGB!");
    }

    params.w = int(first_header->pixel_width);
    params.h = int(first_header->pixel_height);
    params.cube = true;

    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    for (int i = 0; i < 6; ++i) {
#ifndef NDEBUG
        const auto *this_header = reinterpret_cast<const KTXHeader *>(data[i].data());

        // make sure all images have same properties
        if (this_header->pixel_width != first_header->pixel_width) {
            log->Error("Image width mismatch %i, expected %i", int(this_header->pixel_width),
                       int(first_header->pixel_width));
            continue;
        }
        if (this_header->pixel_height != first_header->pixel_height) {
            log->Error("Image height mismatch %i, expected %i", int(this_header->pixel_height),
                       int(first_header->pixel_height));
            continue;
        }
        if (this_header->gl_internal_format != first_header->gl_internal_format) {
            log->Error("Internal format mismatch %i, expected %i", int(this_header->gl_internal_format),
                       int(first_header->gl_internal_format));
            continue;
        }
#endif
        int data_offset = sizeof(KTXHeader);
        int _w = params.w, _h = params.h;

        for (int j = 0; j < int(first_header->mipmap_levels_count); j++) {
            uint32_t img_size;
            memcpy(&img_size, &data[data_offset], sizeof(uint32_t));
            data_offset += sizeof(uint32_t);
            glCompressedTexImage2D(GLenum(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i), j,
                                   GLenum(first_header->gl_internal_format), _w, _h, 0, GLsizei(img_size),
                                   reinterpret_cast<const GLvoid *>(uintptr_t(data_off[i] + data_offset)));
            data_offset += img_size;

            _w = std::max(_w / 2, 1);
            _h = std::max(_h / 2, 1);

            const int pad = (data_offset % 4) ? (4 - (data_offset % 4)) : 0;
            data_offset += pad;
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture2D::ApplySampling(SamplingParams sampling, ILog *log) {
    const auto tex_id = GLuint(handle_.id);

    if (!params.cube) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[size_t(sampling.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[size_t(sampling.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S, g_gl_wrap_mode[size_t(sampling.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T, g_gl_wrap_mode[size_t(sampling.wrap)]);

#ifndef __ANDROID__
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_LOD_BIAS,
                                     bool(params.flags & eTexFlagBits::NoBias) ? 0.0f : sampling.lod_bias.to_float());
#endif
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_LOD, sampling.min_lod.to_float());
        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_LOD, sampling.max_lod.to_float());

        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnisotropyLevel);

        const bool custom_mip_filter = bool(params.flags & (eTexFlagBits::MIPMin | eTexFlagBits::MIPMax));

        if (!IsCompressedFormat(params.format) &&
            (sampling.filter == eTexFilter::Trilinear || sampling.filter == eTexFilter::Bilinear) &&
            !custom_mip_filter) {
            if (!initialized_mips_) {
                log->Error("Error generating mips from uninitilized data!");
            } else if (initialized_mips_ != (1u << 0)) {
                log->Warning("Overriding initialized mips!");
            }

            ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_2D, tex_id);

            const int mip_count = CalcMipCount(params.w, params.h, 1, sampling.filter);
            initialized_mips_ = (1u << mip_count) - 1;
        }

        if (sampling.compare != eTexCompare::None) {
            assert(IsDepthFormat(params.format));
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_FUNC,
                                         g_gl_compare_func[size_t(sampling.compare)]);
        } else {
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
    } else {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_gl_min_filter[size_t(sampling.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_gl_mag_filter[size_t(sampling.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_S,
                                     g_gl_wrap_mode[size_t(sampling.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_T,
                                     g_gl_wrap_mode[size_t(sampling.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_R,
                                     g_gl_wrap_mode[size_t(sampling.wrap)]);

        if (!IsCompressedFormat(params.format) &&
            (sampling.filter == eTexFilter::Trilinear || sampling.filter == eTexFilter::Bilinear)) {
            ren_glGenerateTextureMipmap_Comp(GL_TEXTURE_CUBE_MAP, tex_id);
        }
    }

    params.sampling = sampling;
}

void Ren::Texture2D::SetSubImage(const int level, const int offsetx, const int offsety, const int sizex,
                                 const int sizey, const eTexFormat format, const void *data, const int data_len) {
    assert(format == params.format);
    assert(params.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params.h >> level, 1));

    if (IsCompressedFormat(format)) {
        ren_glCompressedTextureSubImage2D_Comp(
            GL_TEXTURE_2D, GLuint(handle_.id), GLint(level), GLint(offsetx), GLint(offsety), GLsizei(sizex),
            GLsizei(sizey), GLInternalFormatFromTexFormat(format, bool(params.flags & eTexFlagBits::SRGB)),
            GLsizei(data_len), data);
    } else {
        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), level, offsetx, offsety, sizex, sizey,
                                     GLFormatFromTexFormat(format), GLTypeFromTexFormat(format), data);
    }

    if (offsetx == 0 && offsety == 0 && sizex == std::max(params.w >> level, 1) &&
        sizey == std::max(params.h >> level, 1)) {
        // consider this level initialized
        initialized_mips_ |= (1u << level);
    }
}

Ren::SyncFence Ren::Texture2D::SetSubImage(const int level, const int offsetx, const int offsety, const int sizex,
                                           const int sizey, const eTexFormat format, const Buffer &sbuf,
                                           CommandBuffer cmd_buf, const int data_off, const int data_len) {
    assert(format == params.format);
    assert(params.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params.h >> level, 1));

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    if (IsCompressedFormat(format)) {
        ren_glCompressedTextureSubImage2D_Comp(
            GL_TEXTURE_2D, GLuint(handle_.id), GLint(level), GLint(offsetx), GLint(offsety), GLsizei(sizex),
            GLsizei(sizey), GLInternalFormatFromTexFormat(format, bool(params.flags & eTexFlagBits::SRGB)),
            GLsizei(data_len), reinterpret_cast<const void *>(uintptr_t(data_off)));
    } else {
        ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), level, offsetx, offsety, sizex, sizey,
                                     GLFormatFromTexFormat(format), GLTypeFromTexFormat(format),
                                     reinterpret_cast<const void *>(uintptr_t(data_off)));
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    if (offsetx == 0 && offsety == 0 && sizex == std::max(params.w >> level, 1) &&
        sizey == std::max(params.h >> level, 1)) {
        // consider this level initialized
        initialized_mips_ |= (1u << level);
    }

    return MakeFence();
}

void Ren::Texture2D::CopyTextureData(const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len) const {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(sbuf.id()));

    if (IsCompressedFormat(params.format)) {
        glGetCompressedTextureImage(GLuint(handle_.id), 0, GLsizei(data_len),
                                    reinterpret_cast<GLvoid *>(uintptr_t(data_off)));
    } else {
        glGetTextureImage(GLuint(handle_.id), 0, GLFormatFromTexFormat(params.format),
                          GLTypeFromTexFormat(params.format), GLsizei(data_len),
                          reinterpret_cast<GLvoid *>(uintptr_t(data_off)));
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void Ren::CopyImageToImage(CommandBuffer cmd_buf, Texture2D &src_tex, const uint32_t src_level, const uint32_t src_x,
                           const uint32_t src_y, Texture2D &dst_tex, const uint32_t dst_level, const uint32_t dst_x,
                           const uint32_t dst_y, const uint32_t dst_face, const uint32_t width, const uint32_t height) {
    glCopyImageSubData(GLuint(src_tex.id()), GL_TEXTURE_2D, GLint(src_level), GLint(src_x), GLint(src_y), 0,
                       GLuint(dst_tex.id()), GL_TEXTURE_2D, GLint(dst_level), GLint(dst_x), GLint(dst_y),
                       GLint(dst_face), GLsizei(width), GLsizei(height), 1);
}

void Ren::ClearImage(Texture2D &tex, const float rgba[4], CommandBuffer cmd_buf) {
    if (IsDepthStencilFormat(tex.params.format) || IsUnsignedIntegerFormat(tex.params.format)) {
        glClearTexImage(tex.id(), 0, GLFormatFromTexFormat(tex.params.format), GLTypeFromTexFormat(tex.params.format),
                        rgba);
    } else {
        glClearTexImage(tex.id(), 0, GL_RGBA, GL_FLOAT, rgba);
    }
}

////////////////////////////////////////////////////////////////////////////////////////

Ren::Texture1D::Texture1D(std::string_view name, const BufferRef &buf, const eTexFormat format, const uint32_t offset,
                          const uint32_t size, ILog *log)
    : name_(name) {
    Init(buf, format, offset, size, log);
}

Ren::Texture1D::~Texture1D() { Free(); }

Ren::Texture1D &Ren::Texture1D::operator=(Texture1D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Free();

    handle_ = std::exchange(rhs.handle_, {});
    buf_ = std::move(rhs.buf_);
    params_ = std::exchange(rhs.params_, {});
    name_ = std::move(rhs.name_);

    RefCounter::operator=(std::move(rhs));

    return (*this);
}

void Ren::Texture1D::Init(const BufferRef &buf, const eTexFormat format, const uint32_t offset, const uint32_t size,
                          ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_BUFFER, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    glBindTexture(GL_TEXTURE_BUFFER, tex_id);
    glTexBufferRange(GL_TEXTURE_BUFFER, GLInternalFormatFromTexFormat(format, false /* is_srgb */), GLuint(buf->id()),
                     offset, size);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    handle_ = {uint32_t(tex_id), TextureHandleCounter++};
    buf_ = buf;
    params_.offset = offset;
    params_.size = size;
    params_.format = format;
}

void Ren::Texture1D::Free() {
    if (params_.format != eTexFormat::Undefined) {
        auto tex_id = GLuint(handle_.id);
        glDeleteTextures(1, &tex_id);
        handle_ = {};
    }
}

////////////////////////////////////////////////////////////////////////////////////////

Ren::Texture3D::Texture3D(std::string_view name, ApiContext *ctx, const Tex3DParams &params, MemAllocators *mem_allocs,
                          ILog *log)
    : name_(name), api_ctx_(ctx) {
    Init(params, mem_allocs, log);
}

Ren::Texture3D::~Texture3D() { Free(); }

Ren::Texture3D &Ren::Texture3D::operator=(Texture3D &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    Free();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    params = std::exchange(rhs.params, {});
    name_ = std::move(rhs.name_);

    resource_state = std::exchange(rhs.resource_state, eResState::Undefined);

    return (*this);
}

void Ren::Texture3D::Init(const Tex3DParams &p, MemAllocators *mem_allocs, ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_3D, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    handle_.id = tex_id;
    handle_.generation = TextureHandleCounter++;
    params = p;

    const GLuint internal_format = GLInternalFormatFromTexFormat(p.format, bool(p.flags & eTexFlagBits::SRGB));

    ren_glTextureStorage3D_Comp(GL_TEXTURE_3D, tex_id, 1, internal_format, GLsizei(p.w), GLsizei(p.h), GLsizei(p.d));

    this->resource_state = eResState::Undefined;

    ren_glTextureParameteri_Comp(GL_TEXTURE_3D, tex_id, GL_TEXTURE_MIN_FILTER,
                                 g_gl_min_filter[size_t(p.sampling.filter)]);
    ren_glTextureParameteri_Comp(GL_TEXTURE_3D, tex_id, GL_TEXTURE_MAG_FILTER,
                                 g_gl_mag_filter[size_t(p.sampling.filter)]);

    ren_glTextureParameteri_Comp(GL_TEXTURE_3D, tex_id, GL_TEXTURE_WRAP_S, g_gl_wrap_mode[size_t(p.sampling.wrap)]);
    ren_glTextureParameteri_Comp(GL_TEXTURE_3D, tex_id, GL_TEXTURE_WRAP_T, g_gl_wrap_mode[size_t(p.sampling.wrap)]);
    ren_glTextureParameteri_Comp(GL_TEXTURE_3D, tex_id, GL_TEXTURE_WRAP_R, g_gl_wrap_mode[size_t(p.sampling.wrap)]);
}

void Ren::Texture3D::Free() {
    if (params.format != eTexFormat::Undefined && !bool(params.flags & eTexFlagBits::NoOwnership)) {
        auto tex_id = GLuint(handle_.id);
        glDeleteTextures(1, &tex_id);

        handle_ = {};
        params.format = eTexFormat::Undefined;
    }
}

void Ren::Texture3D::SetSubImage(int offsetx, int offsety, int offsetz, int sizex, int sizey, int sizez,
                                 eTexFormat format, const Buffer &sbuf, CommandBuffer cmd_buf, int data_off,
                                 int data_len) {
    assert(format == params.format);
    assert(offsetx >= 0 && offsetx + sizex <= params.w);
    assert(offsety >= 0 && offsety + sizey <= params.h);
    assert(offsetz >= 0 && offsetz + sizez <= params.d);

    assert(sbuf.type() == eBufType::Upload);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    if (IsCompressedFormat(format)) {
        ren_glCompressedTextureSubImage3D_Comp(
            GL_TEXTURE_3D, GLuint(handle_.id), 0, GLint(offsetx), GLint(offsety), GLint(offsetz), GLsizei(sizex),
            GLsizei(sizey), GLsizei(sizez),
            GLInternalFormatFromTexFormat(format, bool(params.flags & eTexFlagBits::SRGB)), GLsizei(data_len),
            reinterpret_cast<const void *>(uintptr_t(data_off)));
    } else {
        ren_glTextureSubImage3D_Comp(GL_TEXTURE_3D, GLuint(handle_.id), 0, offsetx, offsety, offsetz, sizex, sizey,
                                     sizez, GLFormatFromTexFormat(format), GLTypeFromTexFormat(format),
                                     reinterpret_cast<const void *>(uintptr_t(data_off)));
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

uint32_t Ren::GLFormatFromTexFormat(const eTexFormat format) { return g_gl_formats[size_t(format)]; }

uint32_t Ren::GLInternalFormatFromTexFormat(const eTexFormat format, const bool is_srgb) {
    const uint32_t ret = g_gl_internal_formats[size_t(format)];
    return is_srgb ? ToSRGBFormat(ret) : ret;
}

uint32_t Ren::GLTypeFromTexFormat(const eTexFormat format) { return g_gl_types[size_t(format)]; }

void Ren::GLUnbindTextureUnits(const int start, const int count) {
    for (int i = start; i < start + count; i++) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_ARRAY, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_3D, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, i, 0);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, i, 0);
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
