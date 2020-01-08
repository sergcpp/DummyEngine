#include "ProbeStorage.h"

#include <Ren/GL.h>

ProbeStorage::ProbeStorage() : format_(Ren::Undefined), res_(0), size_(0), capacity_(0) {
}

ProbeStorage::~ProbeStorage() {
    if (tex_id_ != 0xffffffff) {
        GLuint tex_id = (GLuint)tex_id_;
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

void ProbeStorage::Resize(Ren::eTexColorFormat format, int res, int capacity) {
    GLuint tex_id;
    if (tex_id_ == 0xffffffff) {
        glGenTextures(1, &tex_id);
        tex_id_ = (uint32_t)tex_id;
    } else {
        tex_id = (GLuint)tex_id_;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id);
    
    int _res = res;
    int level = 0;
    while (_res >= 16) {
        if (format != Ren::Compressed) {
            glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, Ren::GLInternalFormatFromTexFormat(format), _res, _res,
                         capacity * 6, 0, Ren::GLFormatFromTexFormat(format), Ren::GLTypeFromTexFormat(format), nullptr);
            Ren::CheckError("glTexImage3D");
        } else {
            const int len = ((_res + 3) / 4) * ((_res + 3) / 4) * 16;
#if !defined(__ANDROID__)
            glCompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, _res, _res,
                                   capacity * 6, 0, capacity * 6 * len, nullptr);
#else
            glCompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, _res, _res,
                                   capacity * 6, 0, capacity * 6 * len, nullptr);
#endif
            Ren::CheckError("glCompressedTexImage3D");
        }
        _res = _res / 2;
        level++;
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, level - 1);

    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if !defined(__ANDROID__)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    format_     = format;
    res_        = res;
    capacity_   = capacity;
    max_level_  = level - 1;

    reserved_temp_layer_ = capacity_ - 1;
}

bool ProbeStorage::SetPixelData(const int level, const int layer, const int face, const Ren::eTexColorFormat format,
        const uint8_t *data, const int data_len) {
    if (format_ != format) return false;

    const GLenum tex_format =
#if !defined(__ANDROID__)
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
            GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
#endif

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id_);

    const int _res = int((unsigned)res_ >> (unsigned)level);

    if (format == Ren::Compressed) {
        glCompressedTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, 0, 0, (layer * 6 + face), _res, _res, 1, tex_format, data_len, data);

#ifndef NDEBUG
        for (GLint error = glGetError(); error; error = glGetError()) {
            LOGE("after glCompressedTexSubImage3D glError (0x%x)\n", error);
        }
#endif

#if !defined(NDEBUG) && !defined(__ANDROID__) && 0
        std::unique_ptr<uint8_t[]> temp_buf(new uint8_t[data_len]);
        glGetCompressedTextureSubImage((GLuint)tex_id_, level, 0, 0, (layer * 6 + face), _res, _res, 1, data_len, &temp_buf[0]);
        assert(memcmp(data, &temp_buf[0], data_len) == 0);
#endif
    } else {
        return false;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    Ren::CheckError("glCompressedTexImage3D");
    return true;
}

bool ProbeStorage::GetPixelData(const int level, const int layer, const int face, const int buf_size, uint8_t *out_pixels) const {
#if !defined(__ANDROID__)
    const int mip_res = (res_ >> level);
    if (buf_size < 4 * mip_res * mip_res) return false;

    glGetTextureSubImage((GLuint)tex_id_, level, 0, 0, (layer * 6 + face), mip_res, mip_res, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf_size, out_pixels);
    Ren::CheckError("glGetTextureSubImage");

    return true;
#else
    return false;
#endif
}
