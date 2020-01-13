#include "ProbeStorage.h"

#include <Ren/GL.h>

ProbeStorage::ProbeStorage()
        : format_(Ren::Undefined), res_(0), size_(0), capacity_(0), max_level_(0), reserved_temp_layer_(-1) {
}

ProbeStorage::~ProbeStorage() {
    if (tex_id_) {
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }
}

int ProbeStorage::Allocate() {
    if (!free_indices_.empty()) {
        int ret = free_indices_.back();
        free_indices_.pop_back();
        return ret;
    } else {
        if (size_ < capacity_ - 1) {
            return size_++;
        } else {
            return -1;
        }
    }
}

void ProbeStorage::Free(int i) {
    if (i == size_ - 1) {
        size_--;
    } else {
        free_indices_.push_back(i);
    }
}

void ProbeStorage::Resize(Ren::eTexColorFormat format, int res, int capacity, Ren::ILog *log) {
    if (tex_id_) {
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }

    GLuint tex_id;
    ren_glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &tex_id);

    const GLenum compressed_tex_format =
#if !defined(__ANDROID__)
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
        GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif

    const int mip_count = Ren::CalcMipCount(res, res, 16, Ren::Bilinear);

    // allocate all mip levels
    if (format != Ren::Compressed) {
        ren_glTextureStorage3D_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, mip_count, Ren::GLInternalFormatFromTexFormat(format), res, res, capacity * 6);
    } else {
        ren_glTextureStorage3D_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, mip_count, compressed_tex_format, res, res, capacity * 6);
    }

    // set texture color to black
    for (int level = 0; level < mip_count; level++) {
        const int _res = int((unsigned)res >> (unsigned)level);
        for (int layer = 0; layer < capacity; layer++) {
            for (int face = 0; face < 6; face++) {
                for (int y_off = 0; y_off < _res; y_off += 16) {
                    // TODO: this fixes an error on android (wtf ???)
                    const int len_override = ((16 + 3) / 4) * ((16 + y_off + 3) / 4) * 16;

                    for (int x_off = 0; x_off < _res; x_off += 16) {
                        if (format != Ren::Compressed) {
                            const uint8_t blank_buf[1024] = {};
                            ren_glTextureSubImage3D_Comp(
                                    GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, level, x_off, y_off, (layer * 6 + face), 16, 16, 1,
                                    Ren::GLFormatFromTexFormat(format), GL_UNSIGNED_BYTE, &blank_buf);
                            Ren::CheckError("glTexSubImage2D", log);
                        } else {
                            ren_glCompressedTextureSubImage3D_Comp(
                                    GL_TEXTURE_CUBE_MAP_ARRAY, tex_id, level, x_off, y_off, (layer * 6 + face), 16, 16, 1, compressed_tex_format,
#if defined(__ANDROID__)
                                    /*Ren::_blank_ASTC_block_16x16_8bb_len*/ len_override, Ren::_blank_ASTC_block_16x16_8bb);
#else
                                    Ren::_blank_DXT5_block_16x16_len, Ren::_blank_DXT5_block_16x16);
#endif
                            Ren::CheckError("glCompressedTexSubImage3D", log);
                        }
                    }
                    (void)len_override;
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

    tex_id_     = tex_id;
    format_     = format;
    res_        = res;
    capacity_   = capacity;
    max_level_  = mip_count - 1;

    reserved_temp_layer_ = capacity_ - 1;
}

bool ProbeStorage::SetPixelData(
        const int level, const int layer, const int face, const Ren::eTexColorFormat format,
        const uint8_t *data, const int data_len, Ren::ILog *log) {
    if (format_ != format) return false;

    const GLenum tex_format =
#if !defined(__ANDROID__)
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
            GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif

    const int _res = int((unsigned)res_ >> (unsigned)level);

    if (format == Ren::Compressed) {
        ren_glCompressedTextureSubImage3D_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, (GLuint)tex_id_, level, 0, 0, (layer * 6 + face), _res, _res, 1, tex_format, data_len, data);

#if !defined(NDEBUG) && !defined(__ANDROID__) && 0
        std::unique_ptr<uint8_t[]> temp_buf(new uint8_t[data_len]);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id_);
        glGetCompressedTextureSubImage((GLuint)tex_id_, level, 0, 0, (layer * 6 + face), _res, _res, 1, data_len, &temp_buf[0]);
        assert(memcmp(data, &temp_buf[0], data_len) == 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
#endif
    } else {
        return false;
    }

    Ren::CheckError("glCompressedTextureSubImage3D", log);
    return true;
}

bool ProbeStorage::GetPixelData(const int level, const int layer, const int face, const int buf_size, uint8_t *out_pixels, Ren::ILog *log) const {
#if !defined(__ANDROID__)
    const int mip_res = int((unsigned)res_ >> (unsigned)level);
    if (buf_size < 4 * mip_res * mip_res) return false;

    glGetTextureSubImage((GLuint)tex_id_, level, 0, 0, (layer * 6 + face), mip_res, mip_res, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf_size, out_pixels);
    Ren::CheckError("glGetTextureSubImage", log);

    return true;
#else
    return false;
#endif
}
