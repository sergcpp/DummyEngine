#include "ProbeStorage.h"

#include "Context.h"
#include "GL.h"
#include "Utils.h"

Ren::ProbeStorage::ProbeStorage()
    : format_(eTexFormat::Undefined), res_(0), size_(0), capacity_(0), max_level_(0), reserved_temp_layer_(-1) {}

Ren::ProbeStorage::~ProbeStorage() { Destroy(); }

bool Ren::ProbeStorage::Resize(ApiContext *api_ctx, MemAllocators *mem_allocs, const eTexFormat format,
                               const int res, const int capacity, ILog *log) {
    Destroy();

    GLuint tex_id;
    ren_glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &tex_id);

    const GLenum compressed_tex_format =
#if !defined(__ANDROID__)
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
        GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif

    const int mip_count = CalcMipCount(res, res, 16, eTexFilter::Bilinear);

    // allocate all mip levels
    if (IsCompressedFormat(format)) {
        ren_glTextureStorage3D_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, mip_count,
                                    GLInternalFormatFromTexFormat(format, false /* is_srgb */), res, res,
                                    capacity * 6);
    } else {
        ren_glTextureStorage3D_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, mip_count, compressed_tex_format, res, res,
                                    capacity * 6);
    }

    const int blank_block_res = 64;
    uint8_t blank_block[blank_block_res * blank_block_res * 4] = {};
    if (IsCompressedFormat(format)) {
        for (int i = 0; i < (blank_block_res / 4) * (blank_block_res / 4) * 16;) {
#if defined(__ANDROID__)
            memcpy(&blank_block[i], _blank_ASTC_block_4x4, _blank_ASTC_block_4x4_len);
            i += _blank_ASTC_block_4x4_len;
#else
            memcpy(&blank_block[i], _blank_DXT5_block_4x4, _blank_DXT5_block_4x4_len);
            i += _blank_DXT5_block_4x4_len;
#endif
        }
    }

    // set texture color to black
    for (int level = 0; level < mip_count; level++) {
        const int _res = int(unsigned(res) >> unsigned(level)), _init_res = std::min(blank_block_res, _res);
        for (int layer = 0; layer < capacity; layer++) {
            for (int face = 0; face < 6; face++) {
                for (int y_off = 0; y_off < _res; y_off += blank_block_res) {
                    const int buf_len =
#if defined(__ANDROID__)
                        // TODO: '+ y_off' fixes an error on Qualcomm (wtf ???)
                        (_init_res / 4) * ((_init_res + y_off) / 4) * 16;
#else
                        (_init_res / 4) * (_init_res / 4) * 16;
#endif

                    for (int x_off = 0; x_off < _res; x_off += blank_block_res) {
                        if (!IsCompressedFormat(format)) {
                            ren_glTextureSubImage3D_Comp(
                                GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, level, x_off, y_off, (layer * 6 + face), _init_res,
                                _init_res, 1, GLFormatFromTexFormat(format), GL_UNSIGNED_BYTE, blank_block);
                            CheckError("glTexSubImage2D", log);
                        } else {
                            ren_glCompressedTextureSubImage3D_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, level, x_off,
                                                                   y_off, (layer * 6 + face), _init_res, _init_res, 1,
                                                                   compressed_tex_format, buf_len, blank_block);
                            CheckError("glCompressedTexSubImage3D", log);
                        }
                    }
                }
            }
        }
    }

    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, GL_TEXTURE_MAX_LEVEL, mip_count - 1);

    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ren_glTextureParameteri_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    handle_.id = tex_id;
    format_ = format;
    res_ = res;
    capacity_ = capacity;
    max_level_ = mip_count - 1;

    reserved_temp_layer_ = capacity_ - 1;

    return true;
}

bool Ren::ProbeStorage::SetPixelData(const int level, const int layer, const int face, const eTexFormat format,
                                const uint8_t *data, const int data_len, ILog *log) {
    if (format_ != format) {
        return false;
    }

    const GLenum tex_format =
#if !defined(__ANDROID__)
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
        GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif

    const int _res = int(unsigned(res_) >> unsigned(level));

    if (IsCompressedFormat(format)) {
        ren_glCompressedTextureSubImage3D_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, GLuint(handle_.id), level, 0, 0,
                                               (layer * 6 + face), _res, _res, 1, tex_format, data_len, data);

#if !defined(NDEBUG) && !defined(__ANDROID__) && 0
        std::unique_ptr<uint8_t[]> temp_buf(new uint8_t[data_len]);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id_);
        glGetCompressedTextureSubImage((GLuint)tex_id_, level, 0, 0, (layer * 6 + face), _res, _res, 1, data_len,
                                       &temp_buf[0]);
        assert(memcmp(data, &temp_buf[0], data_len) == 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
#endif
    } else {
        return false;
    }

    CheckError("glCompressedTextureSubImage3D", log);
    return true;
}

bool Ren::ProbeStorage::GetPixelData(const int level, const int layer, const int face, const int buf_size,
                                uint8_t *out_pixels, ILog *log) const {
#if !defined(__ANDROID__)
    const int mip_res = int(unsigned(res_) >> unsigned(level));
    if (buf_size < 4 * mip_res * mip_res) {
        return false;
    }

    glGetTextureSubImage(GLuint(handle_.id), level, 0, 0, (layer * 6 + face), mip_res, mip_res, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, buf_size, out_pixels);
    CheckError("glGetTextureSubImage", log);

    return true;
#else
    return false;
#endif
}

void Ren::ProbeStorage::Destroy() {
    if (handle_.id) {
        auto tex_id = GLuint(handle_.id);
        glDeleteTextures(1, &tex_id);
    }
}

void Ren::ProbeStorage::Finalize() {}
