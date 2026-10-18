#include "ImageGL.h"

#include <memory>

#include "../Config.h"
#include "../Context.h"
#include "../utils/Utils.h"
#include "GL.h"
#include "GLCtx.h"

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
#include "../Format.inl"
};
#undef X

extern const uint32_t g_compare_func_gl[];

extern const uint32_t g_min_filter_gl[];
extern const uint32_t g_mag_filter_gl[];
extern const uint32_t g_wrap_mode_gl[];

extern const float AnisotropyLevel;
} // namespace Ren

static_assert(sizeof(GLsync) == sizeof(void *));

bool Ren::Image_Init(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, String name, const ImgParams &p,
                     const BufferMain *sbuf_main, const BufferCold *sbuf_cold, uint32_t data_off, CommandBuffer cmd_buf,
                     MemAllocators *mem_allocs, ILog *log) {
    img_main.generation = api.image_counter++;
    // img_main.views.push_back({});
    img_cold.name = std::move(name);
    img_cold.params = p;

    if (!img_cold.params.mip_count) {
        img_cold.params.mip_count = CalcMipCount(p.w, p.h, 1);
    }

    GLuint tex_id;
    if (p.flags & eImgFlags::CubeMap) {
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex_id);
    } else {
        glCreateTextures(p.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE
                                       : ((p.flags & eImgFlags::Array) ? GL_TEXTURE_2D_ARRAY
                                                                       : (p.d ? GL_TEXTURE_3D : GL_TEXTURE_2D)),
                         1, &tex_id);
    }
#ifdef REN_ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, img_cold.name.c_str());
#endif
    img_main.img = tex_id;

    const auto format = (GLenum)GLFormatFromFormat(p.format),
               internal_format = (GLenum)GLInternalFormatFromFormat(p.format),
               type = (GLenum)GLTypeFromFormat(p.format);

    if (internal_format != 0xffffffff) {
        if (p.samples > 1) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex_id);
            glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, GLsizei(p.samples), internal_format, GLsizei(p.w),
                                      GLsizei(p.h), GL_TRUE);
        } else {
            if (!(p.flags & eImgFlags::Array)) {
                if (p.flags & eImgFlags::CubeMap) {
                    ren_glTextureStorage2D_Comp(GL_TEXTURE_CUBE_MAP, tex_id, img_cold.params.mip_count, internal_format,
                                                p.w, p.h);
                } else if (p.d == 0) {
                    ren_glTextureStorage2D_Comp(GL_TEXTURE_2D, tex_id, img_cold.params.mip_count, internal_format,
                                                GLsizei(p.w), GLsizei(p.h));
                } else {
                    ren_glTextureStorage3D_Comp(GL_TEXTURE_3D, tex_id, img_cold.params.mip_count, internal_format,
                                                GLsizei(p.w), GLsizei(p.h), GLsizei(p.d));
                }
            } else {
                ren_glTextureStorage3D_Comp(GL_TEXTURE_2D_ARRAY, tex_id, img_cold.params.mip_count, internal_format,
                                            GLsizei(p.w), GLsizei(p.h), GLsizei(p.d));
            }
            if (sbuf_main) {
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf_main->buf);

                int bytes_left = sbuf_cold->size - data_off;
                if (p.flags & eImgFlags::CubeMap) {
                    for (int i = 0; i < 6; ++i) {
                        int w = p.w, h = p.h;
                        for (int j = 0; j < img_cold.params.mip_count; ++j) {
                            const int len = GetDataLenBytes(w, h, 1, p.format);
                            if (len > bytes_left) {
                                log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
                                return false;
                            }

                            ren_glTextureSubImage3D_Comp(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex_id, j, 0, 0, i, w, h,
                                                         1, format, type,
                                                         reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
                            data_off += len;
                            bytes_left -= len;

                            w = std::max(w / 2, 1);
                            h = std::max(h / 2, 1);
                        }
                    }
                } else {
                    int w = p.w, h = p.h, d = p.d;
                    for (int i = 0; i < img_cold.params.mip_count; ++i) {
                        const int len = GetDataLenBytes(w, h, d, p.format);
                        if (len > bytes_left) {
                            log->Error("Insufficient data length, bytes left %i, expected %i", bytes_left, len);
                            return false;
                        }

                        if (p.d == 0) {
                            if (IsCompressedFormat(p.format)) {
                                ren_glCompressedTextureSubImage2D_Comp(
                                    GL_TEXTURE_2D, GLuint(img_main.img), i, 0, 0, w, h, internal_format, len,
                                    reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
                            } else {
                                ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, tex_id, i, 0, 0, w, h, format, type,
                                                             reinterpret_cast<const GLvoid *>(uintptr_t(data_off)));
                            }
                        } else {
                            if (IsCompressedFormat(p.format)) {
                                ren_glCompressedTextureSubImage3D_Comp(
                                    GL_TEXTURE_3D, GLuint(img_main.img), i, 0, 0, 0, w, h, d, internal_format, len,
                                    reinterpret_cast<const void *>(uintptr_t(data_off)));
                            } else {
                                ren_glTextureSubImage3D_Comp(GL_TEXTURE_3D, GLuint(img_main.img), i, 0, 0, 0, w, h, d,
                                                             format, type,
                                                             reinterpret_cast<const void *>(uintptr_t(data_off)));
                            }
                        }

                        data_off += len;
                        bytes_left -= len;
                        w = std::max(w / 2, 1);
                        h = std::max(h / 2, 1);
                        d = std::max(d / 2, 1);
                    }
                }

                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }
        }
    }

    if (IsDepthStencilFormat(p.format)) {
        // create additional 'depth-only' image view (for compatibility with VK)
        img_main.views.push_back(tex_id);
    }

    return Image_SetSampling(api, img_main, img_cold, p.sampling, log);
}

bool Ren::Image_Init(const ApiContext &api, ImageCold &img_cold, String name, const ImgParams &p, MemAllocation &&alloc,
                     ILog *log) {
    img_cold.name = std::move(name);
    img_cold.params = p;

    return true;
}

void Ren::Image_Destroy(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold) {
    if (img_cold.params.format != eFormat::Undefined &&
        !(Bitmask<eImgFlags>{img_cold.params.flags} & eImgFlags::NoOwnership)) {
        auto tex_id = GLuint(img_main.img);
        glDeleteTextures(1, &tex_id);
        for (uint32_t view : img_main.views) {
            if (view != tex_id) {
                auto view_id = GLuint(view);
                glDeleteTextures(1, &view_id);
            }
        }
    }
    img_main = {};
    img_cold = {};
}

void Ren::Image_DestroyImmediately(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold) {
    Image_Destroy(api, img_main, img_cold);
}

bool Ren::Image_SetSampling(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, const SamplingParams s,
                            ILog *log) {
    const auto tex_id = GLuint(img_main.img);

    if (!(Bitmask<eImgFlags>{img_cold.params.flags} & eImgFlags::CubeMap)) {
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_FILTER, g_min_filter_gl[size_t(s.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAG_FILTER, g_mag_filter_gl[size_t(s.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_S, g_wrap_mode_gl[size_t(s.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_T, g_wrap_mode_gl[size_t(s.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_WRAP_R, g_wrap_mode_gl[size_t(s.wrap)]);

        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_LOD_BIAS, s.lod_bias.to_float());
        // ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MIN_LOD, s.min_lod.to_float());
        // ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_LOD, s.max_lod.to_float());

        ren_glTextureParameterf_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnisotropyLevel);

        if (s.compare != eCompareOp::None) {
            assert(IsDepthFormat(img_cold.params.format));
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_FUNC,
                                         g_compare_func_gl[size_t(s.compare)]);
        } else {
            ren_glTextureParameteri_Comp(GL_TEXTURE_2D, tex_id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
    } else {
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MIN_FILTER,
                                     g_min_filter_gl[size_t(s.filter)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_MAG_FILTER,
                                     g_mag_filter_gl[size_t(s.filter)]);

        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_S, g_wrap_mode_gl[size_t(s.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_T, g_wrap_mode_gl[size_t(s.wrap)]);
        ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP, tex_id, GL_TEXTURE_WRAP_R, g_wrap_mode_gl[size_t(s.wrap)]);
    }

    img_cold.params.sampling = s;

    return true;
}

void Ren::Image_SetSubImage(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, const int layer,
                            const int level, const Vec3i &offset, const Vec3i &size, const eFormat format,
                            const BufferMain &sbuf_main, CommandBuffer cmd_buf, const int data_off,
                            const int data_len) {
    assert(format == img_cold.params.format);
    assert(img_cold.params.samples == 1);
    assert(offset[0] >= 0 && offset[0] + size[0] <= std::max(img_cold.params.w >> level, 1));
    assert(offset[1] >= 0 && offset[1] + size[1] <= std::max(img_cold.params.h >> level, 1));
    assert(offset[2] >= 0 && offset[2] + size[2] <= std::max(img_cold.params.d >> level, 1));

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf_main.buf);

    if (!(Bitmask<eImgFlags>{img_cold.params.flags} & eImgFlags::Array)) {
        if (img_cold.params.d == 0) {
            if (IsCompressedFormat(format)) {
                ren_glCompressedTextureSubImage2D_Comp(
                    GL_TEXTURE_2D, GLuint(img_main.img), GLint(level), GLint(offset[0]), GLint(offset[1]),
                    GLsizei(size[0]), GLsizei(size[1]), GLInternalFormatFromFormat(format), GLsizei(data_len),
                    reinterpret_cast<const void *>(uintptr_t(data_off)));
            } else {
                ren_glTextureSubImage2D_Comp(GL_TEXTURE_2D, GLuint(img_main.img), level, offset[0], offset[1], size[0],
                                             size[1], GLFormatFromFormat(format), GLTypeFromFormat(format),
                                             reinterpret_cast<const void *>(uintptr_t(data_off)));
            }
        } else {
            if (IsCompressedFormat(format)) {
                ren_glCompressedTextureSubImage3D_Comp(
                    GL_TEXTURE_3D, GLuint(img_main.img), 0, GLint(offset[0]), GLint(offset[1]), GLint(offset[2]),
                    GLsizei(size[0]), GLsizei(size[1]), GLsizei(size[2]), GLInternalFormatFromFormat(format),
                    GLsizei(data_len), reinterpret_cast<const void *>(uintptr_t(data_off)));
            } else {
                ren_glTextureSubImage3D_Comp(GL_TEXTURE_3D, GLuint(img_main.img), 0, offset[0], offset[1], offset[2],
                                             size[0], size[1], size[2], GLFormatFromFormat(format),
                                             GLTypeFromFormat(format),
                                             reinterpret_cast<const void *>(uintptr_t(data_off)));
            }
        }
    } else {
        if (IsCompressedFormat(format)) {
            ren_glCompressedTextureSubImage3D_Comp(GL_TEXTURE_2D_ARRAY, GLuint(img_main.img), 0, GLint(offset[0]),
                                                   GLint(offset[1]), GLint(layer + offset[2]), GLsizei(size[0]),
                                                   GLsizei(size[1]), GLsizei(size[2]),
                                                   GLInternalFormatFromFormat(format), GLsizei(data_len),
                                                   reinterpret_cast<const void *>(uintptr_t(data_off)));
        } else {
            ren_glTextureSubImage3D_Comp(GL_TEXTURE_2D_ARRAY, GLuint(img_main.img), 0, offset[0], offset[1],
                                         layer + offset[2], size[0], size[1], size[2], GLFormatFromFormat(format),
                                         GLTypeFromFormat(format), reinterpret_cast<const void *>(uintptr_t(data_off)));
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

int Ren::Image_AddView(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, const eFormat format,
                       const int mip_level, const int mip_count, const int base_layer, const int layer_count) {
    GLuint tex_view;
    glGenTextures(1, &tex_view);
    glTextureView(tex_view, GL_TEXTURE_2D, img_main.img, GLInternalFormatFromFormat(format), mip_level, mip_count,
                  base_layer, layer_count);
#ifdef REN_ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_view, -1, img_cold.name.c_str());
#endif
    img_main.views.push_back(tex_view);

    return int(img_main.views.size()) - 1;
}

void Ren::Image_CmdClear(const ApiContext &api, ImageMain &img_main, ImageCold &img_cold, const ClearColor &col,
                         CommandBuffer cmd_buf) {
    glClearTexImage(img_main.img, 0, GLFormatFromFormat(img_cold.params.format),
                    GLTypeFromFormat(img_cold.params.format), col.uint32);
}

void Ren::Image_CmdCopyToBuffer(const ApiContext &api, const ImageMain &img_main, const ImageCold &img_cold,
                                BufferMain &buf_main, BufferCold &buf_cold, CommandBuffer cmd_buf,
                                const uint32_t data_off) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(buf_main.buf));

    if (IsCompressedFormat(img_cold.params.format)) {
        glGetCompressedTextureImage(GLuint(img_main.img), 0, GLsizei(buf_cold.size - data_off),
                                    reinterpret_cast<GLvoid *>(uintptr_t(data_off)));
    } else {
        glGetTextureImage(GLuint(img_main.img), 0, GLFormatFromFormat(img_cold.params.format),
                          GLTypeFromFormat(img_cold.params.format), GLsizei(buf_cold.size - data_off),
                          reinterpret_cast<GLvoid *>(uintptr_t(data_off)));
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void Ren::Image_CmdCopyToImage(const ApiContext &api, CommandBuffer cmd_buf, const ImageMain &src_main,
                               const ImageCold &src_cold, const uint32_t src_level, const Vec3i &src_offset,
                               const ImageMain &dst_main, const ImageCold &dst_cold, const uint32_t dst_level,
                               const Vec3i &dst_offset, const uint32_t dst_face, const Vec3i &size) {
    glCopyImageSubData(GLuint(src_main.img), GL_TEXTURE_2D, GLint(src_level), src_offset[0], src_offset[1],
                       src_offset[2], GLuint(dst_main.img), GL_TEXTURE_2D, GLint(dst_level), dst_offset[0],
                       dst_offset[1], dst_offset[2], GLsizei(size[0]), GLsizei(size[1]), GLsizei(size[2]));
}

////////////////////////////////////////////////////////////////////////////////////////

uint32_t Ren::GLFormatFromFormat(const eFormat format) { return g_formats_gl[size_t(format)].format; }

uint32_t Ren::GLInternalFormatFromFormat(const eFormat format) { return g_formats_gl[size_t(format)].internal_format; }

uint32_t Ren::GLTypeFromFormat(const eFormat format) { return g_formats_gl[size_t(format)].type; }

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
