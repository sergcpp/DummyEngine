#include "TextureGL.h"

#include <memory>

#include "SOIL2/SOIL2.h"

#include "GL.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
std::unique_ptr<uint8_t[]> ReadTGAFile(const void *data, int &w, int &h, eTexColorFormat &format);
void CheckError(const char *op);

uint32_t g_gl_formats[] = {
    0xffffffff,     // Undefined
    GL_RGB,         // RawRGB888
    GL_RGBA,        // RawRGBA8888
    GL_LUMINANCE,   // RawLUM8
    GL_RED,         // RawR32F
    GL_RED,         // RawR16F
    GL_RED,         // RawR8
    GL_RG,          // RawRG88
    GL_RGB,         // RawRGB32F
    GL_RGBA,        // RawRGBA32F
    0xffffffff,     // RawRGBE8888
    GL_RGB,         // RawRGB16F
    GL_RGBA,        // RawRGBA16F
    GL_RG,          // RawRG16F
    GL_RG,          // RawRG32F
    0xffffffff,     // Compressed
    0xffffffff,     // None
};
static_assert(sizeof(g_gl_formats) / sizeof(g_gl_formats[0]) == FormatCount, "!");

uint32_t g_gl_internal_formats[] = {
    0xffffffff,     // Undefined
    GL_RGB8,        // RawRGB888
    GL_RGBA8,       // RawRGBA8888
    GL_LUMINANCE,   // RawLUM8
    GL_R32F,        // RawR32F
    GL_R16F,        // RawR16F
    GL_R8,          // RawR8
    GL_RG8,         // RawRG88
    GL_RGB32F,      // RawRGB32F
    GL_RGBA32F,     // RawRGBA32F
    0xffffffff,     // RawRGBE8888
    GL_RGB16F,      // RawRGB16F
    GL_RGBA16F,     // RawRGBA16F
    GL_RG16F,       // RawRG16F
    GL_RG32F,       // RawRG32F
    0xffffffff,     // Compressed
    0xffffffff,     // None
};
static_assert(sizeof(g_gl_internal_formats) / sizeof(g_gl_internal_formats[0]) == FormatCount, "!");

uint32_t g_gl_types[] = {
    0xffffffff,         // Undefined
    GL_UNSIGNED_BYTE,   // RawRGB888
    GL_UNSIGNED_BYTE,   // RawRGBA8888
    GL_UNSIGNED_BYTE,   // RawLUM8
    GL_FLOAT,           // RawR32F
    GL_HALF_FLOAT,      // RawR16F
    GL_UNSIGNED_BYTE,   // RawR8
    GL_UNSIGNED_BYTE,   // RawRG88
    GL_FLOAT,           // RawRGB32F
    GL_FLOAT,           // RawRGBA32F
    0xffffffff,         // RawRGBE8888
    GL_HALF_FLOAT,      // RawRGB16F
    GL_HALF_FLOAT,      // RawRGBA16F
    GL_HALF_FLOAT,      // RawRG16F
    GL_FLOAT,           // RawRG32F
    0xffffffff,         // Compressed
    0xffffffff,         // None
};
static_assert(sizeof(g_gl_types) / sizeof(g_gl_types[0]) == FormatCount, "!");
}

Ren::Texture2D::Texture2D(const char *name, const void *data, int size,
                          const Texture2DParams &p, eTexLoadStatus *load_status) {
    Init(name, data, size, p, load_status);
}

Ren::Texture2D::Texture2D(const char *name, const void *data[6], const int size[6],
                          const Texture2DParams &p, eTexLoadStatus *load_status) {
    Init(name, data, size, p, load_status);
}

Ren::Texture2D::~Texture2D() {
    if (params_.format != Undefined) {
        GLuint gl_tex = (GLuint)tex_id_;
        glDeleteTextures(1, &gl_tex);
    }
}

Ren::Texture2D &Ren::Texture2D::operator=(Ren::Texture2D &&rhs) {
    if (this == &rhs) return *this;

    if (params_.format != Undefined) {
        GLuint gl_tex = (GLuint)tex_id_;
        glDeleteTextures(1, &gl_tex);
    }

    RefCounter::operator=(std::move(rhs));

    tex_id_ = rhs.tex_id_;
    rhs.tex_id_ = 0;
    params_ = rhs.params_;
    rhs.params_ = {};
    ready_ = rhs.ready_;
    rhs.ready_ = false;
    cubemap_ready_ = rhs.cubemap_ready_;
    rhs.cubemap_ready_ = 0;
    strcpy(name_, rhs.name_);
    rhs.name_[0] = '\0';
    return *this;
}

void Ren::Texture2D::Init(const char *name, const void *data, int size,
                          const Texture2DParams &p, eTexLoadStatus *load_status) {
    strcpy(name_, name);

    if (!data) {
        unsigned char cyan[3] = { 0, 255, 255 };
        Texture2DParams _p;
        _p.w = _p.h = 1;
        _p.format = RawRGB888;
        _p.filter = NoFilter;
        _p.repeat = Repeat;
        InitFromRAWData(cyan, _p);
        // mark it as not ready
        ready_ = false;
        if (load_status) *load_status = TexCreatedDefault;
    } else {
        if (strstr(name, ".tga_rgbe") != 0 || strstr(name, ".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, p);
        } else if (strstr(name, ".tga") != 0 || strstr(name, ".TGA") != 0) {
            InitFromTGAFile(data, p);
        } else if (strstr(name, ".dds") != 0 || strstr(name, ".DDS") != 0) {
            InitFromDDSFile(data, size, p);
        } else if (strstr(name, ".png") != 0 || strstr(name, ".PNG") != 0) {
            InitFromPNGFile(data, size, p);
        } else {
            InitFromRAWData(data, p);
        }
        ready_ = true;
        if (load_status) *load_status = TexCreatedFromData;
    }
}

void Ren::Texture2D::Init(const char *name, const void *data[6], const int size[6],
                          const Texture2DParams &p, eTexLoadStatus *load_status) {
    strcpy(name_, name);

    if (!data) {
        const unsigned char cyan[3] = { 0, 255, 255 };
        const void *data[6] = { cyan, cyan, cyan, cyan, cyan, cyan };
        Texture2DParams _p;
        _p.w = _p.h = 1;
        _p.format = RawRGB888;
        _p.filter = NoFilter;
        _p.repeat = Repeat;
        InitFromRAWData(data, _p);
        // mark it as not ready
        ready_ = false;
        cubemap_ready_ = 0;
        if (load_status) *load_status = TexCreatedDefault;
    } else {
        if (strstr(name, ".tga_rgbe") != 0 || strstr(name, ".TGA_RGBE") != 0) {
            InitFromTGA_RGBEFile(data, p);
        } else if (strstr(name, ".tga") != 0 || strstr(name, ".TGA") != 0) {
            InitFromTGAFile(data, p);
        } else if (strstr(name, ".png") != 0 || strstr(name, ".PNG") != 0) {
            InitFromPNGFile(data, size, p);
        } else if (strstr(name, ".dds") != 0 || strstr(name, ".DDS") != 0) {
            InitFromDDSFile(data, size, p);
        } else {
            InitFromRAWData(data, p);
        }

        ready_ = (cubemap_ready_ & (1 << 0)) == 1;
        for (int i = 1; i < 6; i++) {
            ready_ = ready_ && ((cubemap_ready_ & (1 << i)) == 1);
        }
        if (load_status) *load_status = TexCreatedFromData;
    }
}

void Ren::Texture2D::InitFromRAWData(const void *data, const Texture2DParams &p) {
    GLuint tex_id;

    if (params_.format == Undefined) {
        glGenTextures(1, &tex_id);
        tex_id_ = tex_id;
    } else {
        tex_id = (GLuint)tex_id_;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    params_ = p;

    GLenum format = (GLenum)GLFormatFromTexFormat(p.format),
           internal_format = (GLenum)GLInternalFormatFromTexFormat(p.format),
           type = (GLenum)GLTypeFromTexFormat(p.format);

    if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, p.w, p.h, 0, format, type, data);
    }

    float anisotropy = 4.0f;
    //glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
    if (anisotropy > 0.0f) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
    }

    if (p.filter == NoFilter) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else if (p.filter == Bilinear) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (p.filter == Trilinear) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (p.filter == BilinearNoMipmap) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (p.repeat == Repeat) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else if (p.repeat == ClampToEdge) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (p.filter == Trilinear || p.filter == Bilinear) {
        //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    CheckError("create texture");
}

void Ren::Texture2D::InitFromTGAFile(const void *data, const Texture2DParams &p) {
    int w = 0, h = 0;
    eTexColorFormat format = Undefined;
    auto image_data = ReadTGAFile(data, w, h, format);

    Texture2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(image_data.get(), _p);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data, const Texture2DParams &p) {
    int w = 0, h = 0;
    eTexColorFormat format = Undefined;
    auto image_data = ReadTGAFile(data, w, h, format);

    std::unique_ptr<uint16_t[]> fp_data = ConvertRGBE_to_RGB16F(image_data.get(), w, h);

    Texture2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = RawRGB16F;

    InitFromRAWData(fp_data.get(), _p);
}

void Ren::Texture2D::InitFromDDSFile(const void *data, int size, const Texture2DParams &p) {
    GLuint tex_id;
    if (params_.format == Undefined) {
        glGenTextures(1, &tex_id);
        tex_id_ = tex_id;
    } else {
        tex_id = (GLuint)tex_id_;
    }

    params_ = p;
    params_.format = Compressed;

    int res = SOIL_load_OGL_texture_from_memory((unsigned char *)data, size, SOIL_LOAD_AUTO, tex_id, SOIL_FLAG_DDS_LOAD_DIRECT);
    assert(res == tex_id);

    GLint w, h;
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

    params_.w = (int)w;
    params_.h = (int)h;

    ChangeFilter(p.filter, p.repeat);
}

void Ren::Texture2D::InitFromPNGFile(const void *data, int size, const Texture2DParams &p) {
    GLuint tex_id;
    if (params_.format == Undefined) {
        glGenTextures(1, &tex_id);
        tex_id_ = tex_id;
    } else {
        tex_id = (GLuint)tex_id_;
    }

    params_ = p;
    params_.format = Compressed;

    int res = SOIL_load_OGL_texture_from_memory((unsigned char *)data, size, SOIL_LOAD_AUTO, tex_id, SOIL_FLAG_INVERT_Y | SOIL_FLAG_GL_MIPMAPS);
    assert(res == tex_id);

    GLint w, h;
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

    params_.w = (int)w;
    params_.h = (int)h;

    ChangeFilter(p.filter, p.repeat);
}

void Ren::Texture2D::InitFromRAWData(const void *data[6], const Texture2DParams &p) {
    assert(p.w > 0 && p.h > 0);
    GLuint tex_id;
    if (params_.format == Undefined) {
        glGenTextures(1, &tex_id);
        tex_id_ = tex_id;
    } else {
        tex_id = (GLuint)tex_id_;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    params_ = p;

    GLenum format = (GLenum)GLFormatFromTexFormat(params_.format),
           internal_format = (GLenum)GLInternalFormatFromTexFormat(params_.format),
           type = (GLenum)GLTypeFromTexFormat(params_.format);

    int w = p.w, h = p.h;

    for (int i = 0; i < 6; i++) {
        if (!data[i]) {
            continue;
        } else {
            cubemap_ready_ |= (1 << i);
        }

        if (format != 0xffffffff && internal_format != 0xffffffff && type != 0xffffffff) {
            glTexImage2D((GLenum)(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i), 0, internal_format, w, h, 0, format, type, data[i]);
        }
    }

    auto f = params_.filter;
    if (f == NoFilter) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else if (f == Bilinear) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, (cubemap_ready_ == 0x3F) ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (f == Trilinear) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, (cubemap_ready_ == 0x3F) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (f == BilinearNoMipmap) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if !defined(GL_ES_VERSION_2_0) && !defined(__EMSCRIPTEN__)
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
#endif

    if ((f == Trilinear || f == Bilinear) && (cubemap_ready_ == 0x3F)) {
        //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }
}

void Ren::Texture2D::InitFromTGAFile(const void *data[6], const Texture2DParams &p) {
    std::unique_ptr<uint8_t[]> image_data[6];
    const void *_image_data[6] = {};
    int w = 0, h = 0;
    eTexColorFormat format = Undefined;
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

    InitFromRAWData(_image_data, _p);
}

void Ren::Texture2D::InitFromTGA_RGBEFile(const void *data[6], const Texture2DParams &p) {
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
    _p.format = Ren::RawRGB16F;

    InitFromRAWData(_image_data, _p);
}

void Ren::Texture2D::InitFromPNGFile(const void *data[6], const int size[6], const Texture2DParams &p) {
    GLuint tex_id;
    if (params_.format == Undefined) {
        glGenTextures(1, &tex_id);
        tex_id_ = tex_id;
    } else {
        tex_id = (GLuint)tex_id_;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    params_ = p;

    unsigned int handle =
        SOIL_load_OGL_cubemap_from_memory((const unsigned char *)data[0], size[0],
                                          (const unsigned char *)data[1], size[1],
                                          (const unsigned char *)data[2], size[2],
                                          (const unsigned char *)data[3], size[3],
                                          (const unsigned char *)data[4], size[4],
                                          (const unsigned char *)data[5], size[5], 0, tex_id, SOIL_FLAG_INVERT_Y);
    assert(handle == tex_id);

    GLint w, h;
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_HEIGHT, &h);

    params_.w = (int)w;
    params_.h = (int)h;
    params_.cube = 1;

    ChangeFilter(p.filter, p.repeat);
}

void Ren::Texture2D::InitFromDDSFile(const void *data[6], const int size[6], const Texture2DParams &p) {
    assert(p.w > 0 && p.h > 0);
    GLuint tex_id;
    if (params_.format == Undefined) {
        glGenTextures(1, &tex_id);
        tex_id_ = tex_id;
    } else {
        tex_id = (GLuint)tex_id_;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);

    params_ = p;

    unsigned int handle =
    SOIL_load_OGL_cubemap_from_memory((const unsigned char *)data[0], size[0],
                                      (const unsigned char *)data[1], size[1],
                                      (const unsigned char *)data[2], size[2],
                                      (const unsigned char *)data[3], size[3],
                                      (const unsigned char *)data[4], size[4],
                                      (const unsigned char *)data[5], size[5], 0, tex_id, SOIL_FLAG_DDS_LOAD_DIRECT);
    assert(handle == tex_id);

    params_.cube = 1;

    ChangeFilter(p.filter, p.repeat);
}

void Ren::Texture2D::ChangeFilter(eTexFilter f, eTexRepeat r) {
    glActiveTexture(GL_TEXTURE0);

    GLenum target = params_.cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    glBindTexture(target, tex_id_);

    if (f == NoFilter) {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else if (f == Bilinear) {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (f == Trilinear) {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else if (f == BilinearNoMipmap) {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (r == Repeat) {
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (params_.cube) {
            glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_REPEAT);
        }
    } else if (r == ClampToEdge) {
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (params_.cube) {
            glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
    }

    if (params_.format != Compressed && (f == Trilinear || f == Bilinear)) {
        //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
        glGenerateMipmap(target);
    }
}

void Ren::Texture2D::ReadTextureData(eTexColorFormat format, void *out_data) const {
#if defined(__ANDROID__)
#else
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id_);

    if (format == RawRGBA8888) {
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out_data);
    }
#endif
}

uint32_t Ren::GLFormatFromTexFormat(eTexColorFormat format) {
    return g_gl_formats[format];
}

uint32_t Ren::GLInternalFormatFromTexFormat(eTexColorFormat format) {
    return g_gl_internal_formats[format];
}

uint32_t Ren::GLTypeFromTexFormat(eTexColorFormat format) {
    return g_gl_types[format];
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif