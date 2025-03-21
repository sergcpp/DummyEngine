#include "TextureGL.h"

#include <memory>

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
#define X(_0, _1, _2, _3, _4, _5, _6, _7, _8) {_6, _7, _8},
struct {
    uint32_t format;
    uint32_t internal_format;
    uint32_t type;
} g_formats_gl[] = {
#include "TextureFormat.inl"
};
#undef X

extern const uint32_t g_compare_func_gl[];

uint32_t TextureHandleCounter = 0;

extern const uint32_t g_min_filter_gl[];
extern const uint32_t g_mag_filter_gl[];
extern const uint32_t g_wrap_mode_gl[];

extern const float AnisotropyLevel;
} // namespace Ren

static_assert(sizeof(GLsync) == sizeof(void *), "!");

Ren::Texture::Texture(std::string_view name, ApiContext *api_ctx, const TexParams &p, MemAllocators *, ILog *log)
    : name_(name) {
    Init(p, nullptr, log);
}

Ren::Texture::Texture(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data, const TexParams &p,
                      MemAllocators *, eTexLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, p, nullptr, load_status, log);
}

Ren::Texture::Texture(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> data[6], const TexParams &p,
                      MemAllocators *mem_allocs, eTexLoadStatus *load_status, ILog *log)
    : name_(name) {
    Init(data, p, nullptr, load_status, log);
}

Ren::Texture::~Texture() { Free(); }

Ren::Texture &Ren::Texture::operator=(Texture &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    handle_ = std::exchange(rhs.handle_, {});
    params = std::exchange(rhs.params, {});
    name_ = std::move(rhs.name_);

    return (*this);
}

uint64_t Ren::Texture::GetBindlessHandle() const { return glGetTextureHandleARB(GLuint(handle_.id)); }

void Ren::Texture::Init(const TexParams &p, MemAllocators *, ILog *log) { InitFromRAWData(nullptr, 0, p, log); }

void Ren::Texture::Init(const TexHandle &handle, const TexParams &_params, MemAllocation &&alloc, ILog *log) {
    handle_ = handle;
    params = _params;

    SetSampling(params.sampling);
}

void Ren::Texture::Init(Span<const uint8_t> data, const TexParams &p, MemAllocators *mem_allocs,
                        eTexLoadStatus *load_status, ILog *log) {
    assert(!data.empty());

    auto sbuf = Buffer{"Temp Stage Buf", nullptr, eBufType::Upload, uint32_t(data.size())};
    { // Update staging buffer
        uint8_t *stage_data = sbuf.Map();
        memcpy(stage_data, data.data(), data.size());
        sbuf.Unmap();
    }
    InitFromRAWData(&sbuf, 0, p, log);

    (*load_status) = eTexLoadStatus::CreatedFromData;
}

void Ren::Texture::Init(Span<const uint8_t> data[6], const TexParams &p, MemAllocators *mem_allocs,
                        eTexLoadStatus *load_status, ILog *log) {
    assert(data);

    auto sbuf = Buffer{
        "Temp Stage Buf", nullptr, eBufType::Upload,
        uint32_t(data[0].size() + data[1].size() + data[2].size() + data[3].size() + data[4].size() + data[5].size())};
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

    (*load_status) = eTexLoadStatus::CreatedFromData;
}

void Ren::Texture::Free() {
    if (params.format != eTexFormat::Undefined && !(Bitmask<eTexFlags>{params.flags} & eTexFlags::NoOwnership)) {
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

void Ren::Texture::Realloc(const int w, const int h, int mip_count, const int samples, const eTexFormat format,
                           CommandBuffer cmd_buf, MemAllocators *mem_allocs, ILog *log) {
    GLuint tex_id;
    glCreateTextures(samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    const GLuint internal_format = GLInternalFormatFromTexFormat(format);

    if (!mip_count) {
        mip_count = CalcMipCount(w, h, 1);
    }

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
            glCopyImageSubData(GLuint(handle_.id), GL_TEXTURE_2D, GLint(src_mip), 0, 0, 0, GLuint(new_handle.id),
                               GL_TEXTURE_2D, GLint(dst_mip), 0, 0, 0, GLsizei(std::max(params.w >> src_mip, 1)),
                               GLsizei(std::max(params.h >> src_mip, 1)), 1);
#ifdef TEX_VERBOSE_LOGGING
            log->Info("Copying data mip %i [old] -> mip %i [new]", src_mip, dst_mip);
#endif
        }
    }
    Free();

    handle_ = new_handle;
    params.w = w;
    params.h = h;
    params.mip_count = mip_count;
    params.samples = samples;
    params.format = format;
}

void Ren::Texture::InitFromRAWData(const Buffer *sbuf, int data_off, const TexParams &p, ILog *log) {
    Free();

    GLuint tex_id;
    glCreateTextures(p.samples > 1
                         ? GL_TEXTURE_2D_MULTISAMPLE
                         : ((p.flags & eTexFlags::Array) ? GL_TEXTURE_2D_ARRAY : (p.d ? GL_TEXTURE_3D : GL_TEXTURE_2D)),
                     1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif
    handle_ = {tex_id, TextureHandleCounter++};

    params = p;

    const auto format = (GLenum)GLFormatFromTexFormat(p.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(p.format),
               type = (GLenum)GLTypeFromTexFormat(p.format);

    auto mip_count = GLsizei(p.mip_count);
    if (!mip_count) {
        mip_count = GLsizei(CalcMipCount(p.w, p.h, 1));
    }

    if (internal_format != 0xffffffff) {
        if (p.samples > 1) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex_id);
            glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, GLsizei(p.samples), internal_format, GLsizei(p.w),
                                      GLsizei(p.h), GL_TRUE);
        } else {
            if (!(p.flags & eTexFlags::Array)) {
                if (p.d == 0) {
                    ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, mip_count, internal_format, GLsizei(p.w),
                                                GLsizei(p.h));
                } else {
                    ren_glTextureStorage3D_Comp(GL_TEXTURE_3D, tex_id, 1, internal_format, GLsizei(p.w), GLsizei(p.h),
                                                GLsizei(p.d));
                }
            } else {
                ren_glTextureStorage3D_Comp(GL_TEXTURE_2D_ARRAY, tex_id, mip_count, internal_format, GLsizei(p.w),
                                            GLsizei(p.h), GLsizei(p.d));
            }
            if (sbuf) {
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf->id());

                int w = p.w, h = p.h, d = p.d;
                int bytes_left = sbuf->size() - data_off;
                for (int i = 0; i < mip_count; ++i) {
                    const int len = GetDataLenBytes(w, h, d, p.format);
                    if (len > bytes_left) {
                        log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
                        return;
                    }

                    assert(!(p.flags & eTexFlags::Array));
                    if (p.d == 0) {
                        if (IsCompressedFormat(p.format)) {
                            ren_glCompressedTextureSubImage2D_Comp(
                                GL_TEXTURE_2D, GLuint(handle_.id), i, 0, 0, w, h, internal_format, len,
                                reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
                        } else {
                            ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, i, 0, 0, w, h, format, type,
                                                         reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
                        }
                    } else {
                        if (IsCompressedFormat(p.format)) {
                            ren_glCompressedTextureSubImage3D_Comp(GL_TEXTURE_3D, GLuint(handle_.id), i, 0, 0, 0, w, h,
                                                                   d, internal_format, len,
                                                                   reinterpret_cast<const void *>(uintptr_t(data_off)));
                        } else {
                            ren_glTextureSubImage3D_Comp(GL_TEXTURE_3D, GLuint(handle_.id), i, 0, 0, 0, w, h, d, format,
                                                         type, reinterpret_cast<const void *>(uintptr_t(data_off)));
                        }
                    }

                    data_off += len;
                    bytes_left -= len;
                    w = std::max(w / 2, 1);
                    h = std::max(h / 2, 1);
                    d = std::max(d / 2, 1);
                }

                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }
        }
    }

    if (IsDepthStencilFormat(p.format)) {
        // create additional 'depth-only' image view (for compatibility with VK)
        handle_.views.push_back(tex_id);
    }

    if (p.samples == 1) {
        ApplySampling(p.sampling, log);
    }

    CheckError("create texture", log);
}

void Ren::Texture::InitFromRAWData(const Buffer &sbuf, int data_off[6], const TexParams &p, ILog *log) {
    assert(p.w > 0 && p.h > 0);
    Free();

    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, name_.c_str());
#endif

    handle_ = {tex_id, TextureHandleCounter++};
    params = p;

    auto flags = Bitmask<eTexFlags>{params.flags};
    flags |= eTexFlags::CubeMap;
    params.flags = uint8_t(flags);

    const auto format = (GLenum)GLFormatFromTexFormat(params.format),
               internal_format = (GLenum)GLInternalFormatFromTexFormat(params.format),
               type = (GLenum)GLTypeFromTexFormat(params.format);

    const int w = p.w, h = p.h;
    const int mip_count = CalcMipCount(w, h, 1);
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
                buffer_offset += GetDataLenBytes(_w, _h, 1, p.format);
            }
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ApplySampling(p.sampling, log);
}

void Ren::Texture::ApplySampling(SamplingParams sampling, ILog *log) {
    const auto tex_id = GLuint(handle_.id);

    if (!(Bitmask<eTexFlags>{params.flags} & eTexFlags::CubeMap)) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_min_filter_gl[size_t(sampling.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_mag_filter_gl[size_t(sampling.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S, g_wrap_mode_gl[size_t(sampling.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T, g_wrap_mode_gl[size_t(sampling.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_R, g_wrap_mode_gl[size_t(sampling.wrap)]);

        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_LOD_BIAS, sampling.lod_bias.to_float());
        // ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_LOD, sampling.min_lod.to_float());
        // ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_LOD, sampling.max_lod.to_float());

        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnisotropyLevel);

        if (sampling.compare != eTexCompare::None) {
            assert(IsDepthFormat(params.format));
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_FUNC,
                                         g_compare_func_gl[size_t(sampling.compare)]);
        } else {
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
    } else {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_min_filter_gl[size_t(sampling.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_mag_filter_gl[size_t(sampling.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_S,
                                     g_wrap_mode_gl[size_t(sampling.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_T,
                                     g_wrap_mode_gl[size_t(sampling.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_R,
                                     g_wrap_mode_gl[size_t(sampling.wrap)]);
    }

    params.sampling = sampling;
}

int Ren::Texture::AddImageView(const eTexFormat format, const int mip_level, const int mip_count, const int base_layer,
                               const int layer_count) {
    GLuint tex_view;
    glGenTextures(1, &tex_view);
    glTextureView(tex_view, GL_TEXTURE_2D, handle_.id, GLInternalFormatFromTexFormat(format), mip_level, mip_count,
                  base_layer, layer_count);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_view, -1, name_.c_str());
#endif
    handle_.views.push_back(tex_view);

    return int(handle_.views.size()) - 1;
}

void Ren::Texture::SetSubImage(const int layer, const int level, const int offsetx, const int offsety,
                               const int offsetz, const int sizex, const int sizey, const int sizez,
                               const eTexFormat format, const Buffer &sbuf, CommandBuffer cmd_buf, const int data_off,
                               const int data_len) {
    assert(format == params.format);
    assert(params.samples == 1);
    assert(offsetx >= 0 && offsetx + sizex <= std::max(params.w >> level, 1));
    assert(offsety >= 0 && offsety + sizey <= std::max(params.h >> level, 1));
    assert(offsetz >= 0 && offsetz + sizez <= std::max(params.d >> level, 1));
    assert(sbuf.type() == eBufType::Upload);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    if (!(Bitmask<eTexFlags>{params.flags} & eTexFlags::Array)) {
        if (params.d == 0) {
            if (IsCompressedFormat(format)) {
                ren_glCompressedTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), GLint(level), GLint(offsetx),
                                                       GLint(offsety), GLsizei(sizex), GLsizei(sizey),
                                                       GLInternalFormatFromTexFormat(format), GLsizei(data_len),
                                                       reinterpret_cast<const void *>(uintptr_t(data_off)));
            } else {
                ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(handle_.id), level, offsetx, offsety, sizex, sizey,
                                             GLFormatFromTexFormat(format), GLTypeFromTexFormat(format),
                                             reinterpret_cast<const void *>(uintptr_t(data_off)));
            }
        } else {
            if (IsCompressedFormat(format)) {
                ren_glCompressedTextureSubImage3D_Comp(
                    GL_TEXTURE_3D, GLuint(handle_.id), 0, GLint(offsetx), GLint(offsety), GLint(offsetz),
                    GLsizei(sizex), GLsizei(sizey), GLsizei(sizez), GLInternalFormatFromTexFormat(format),
                    GLsizei(data_len), reinterpret_cast<const void *>(uintptr_t(data_off)));
            } else {
                ren_glTextureSubImage3D_Comp(GL_TEXTURE_3D, GLuint(handle_.id), 0, offsetx, offsety, offsetz, sizex,
                                             sizey, sizez, GLFormatFromTexFormat(format), GLTypeFromTexFormat(format),
                                             reinterpret_cast<const void *>(uintptr_t(data_off)));
            }
        }
    } else {
        if (IsCompressedFormat(format)) {
            ren_glCompressedTextureSubImage3D_Comp(
                GL_TEXTURE_2D_ARRAY, GLuint(handle_.id), 0, GLint(offsetx), GLint(offsety), GLint(layer + offsetz),
                GLsizei(sizex), GLsizei(sizey), GLsizei(sizez), GLInternalFormatFromTexFormat(format),
                GLsizei(data_len), reinterpret_cast<const void *>(uintptr_t(data_off)));
        } else {
            ren_glTextureSubImage3D_Comp(GL_TEXTURE_2D_ARRAY, GLuint(handle_.id), 0, offsetx, offsety, layer + offsetz,
                                         sizex, sizey, sizez, GLFormatFromTexFormat(format),
                                         GLTypeFromTexFormat(format),
                                         reinterpret_cast<const void *>(uintptr_t(data_off)));
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void Ren::Texture::CopyTextureData(const Buffer &sbuf, CommandBuffer cmd_buf, int data_off, int data_len) const {
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

void Ren::CopyImageToImage(CommandBuffer cmd_buf, Texture &src_tex, const uint32_t src_level, const uint32_t src_x,
                           const uint32_t src_y, const uint32_t src_z, Texture &dst_tex, const uint32_t dst_level,
                           const uint32_t dst_x, const uint32_t dst_y, const uint32_t dst_z, const uint32_t dst_face,
                           const uint32_t w, const uint32_t h, const uint32_t d) {
    glCopyImageSubData(GLuint(src_tex.id()), GL_TEXTURE_2D, GLint(src_level), GLint(src_x), GLint(src_y), GLint(src_z),
                       GLuint(dst_tex.id()), GL_TEXTURE_2D, GLint(dst_level), GLint(dst_x), GLint(dst_y), GLint(dst_z),
                       GLsizei(w), GLsizei(h), GLsizei(d));
}

void Ren::ClearImage(const Texture &tex, const float rgba[4], CommandBuffer cmd_buf) {
    if (IsDepthStencilFormat(tex.params.format) || IsUnsignedIntegerFormat(tex.params.format)) {
        glClearTexImage(tex.id(), 0, GLFormatFromTexFormat(tex.params.format), GLTypeFromTexFormat(tex.params.format),
                        rgba);
    } else {
        glClearTexImage(tex.id(), 0, GL_RGBA, GL_FLOAT, rgba);
    }
}

////////////////////////////////////////////////////////////////////////////////////////

uint32_t Ren::GLFormatFromTexFormat(const eTexFormat format) { return g_formats_gl[size_t(format)].format; }

uint32_t Ren::GLInternalFormatFromTexFormat(const eTexFormat format) {
    return g_formats_gl[size_t(format)].internal_format;
}

uint32_t Ren::GLTypeFromTexFormat(const eTexFormat format) { return g_formats_gl[size_t(format)].type; }

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
