#include "TextureSW.h"

#include <memory>

#include "SW/SW.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
std::unique_ptr<uint8_t[]> ReadTGAFile(const void *data, int &w, int &h, eTexFormat &format);
void CheckError(const char *op);
}

Ren::Texture2D::Texture2D(const char *name, const void *data, int size,
                          const Tex2DParams &p, eTexLoadStatus *load_status) {
    Init(name, data, size, p, load_status);
}

Ren::Texture2D::Texture2D(const char *name, const void *data[6], const int size[6],
                          const Tex2DParams &p, eTexLoadStatus *load_status) {
    Init(name, data, size, p, load_status);
}

Ren::Texture2D::~Texture2D() {
    if (params_.format != Undefined) {
        SWint sw_tex = (SWint)tex_id_;
        swDeleteTexture(sw_tex);
    }
}

Ren::Texture2D &Ren::Texture2D::operator=(Ren::Texture2D &&rhs) {
    if (this == &rhs) return *this;

    if (params_.format != Undefined) {
        SWint sw_tex = (SWint)tex_id_;
        swDeleteTexture(sw_tex);
    }

    RefCounter::operator=(std::move(rhs));

    tex_id_ = rhs.tex_id_;
    rhs.tex_id_ = 0;
    params_ = rhs.params_;
    rhs.params_ = {};
    ready_ = rhs.ready_;
    rhs.ready_ = false;
    strcpy(name_, rhs.name_);
    rhs.name_[0] = '\0';
    return *this;
}

void Ren::Texture2D::Init(const char *name, const void *data, int size,
                          const Tex2DParams &p, eTexLoadStatus *load_status) {
    strcpy(name_, name);
    ((void)size);

    if (!data) {
        unsigned char cyan[3] = { 0, 255, 255 };
        Tex2DParams _p;
        _p.w = _p.h = 1;
        _p.format = RawRGB888;
        InitFromRAWData(cyan, _p);
        // mark it as not ready
        ready_ = false;
        if (load_status) *load_status = TexCreatedDefault;
    } else {
        if (strstr(name, ".tga") != 0 || strstr(name, ".TGA") != 0) {
            InitFromTGAFile(data, p);
        } else if (strstr(name, ".tex") != 0 || strstr(name, ".TEX") != 0) {
            InitFromTEXFile(data, p);
        } else {
            InitFromRAWData(data, p);
        }
        ready_ = true;
        if (load_status) *load_status = TexCreatedFromData;
    }
}

void Ren::Texture2D::Init(const char *name, const void *data[6], const int size[6],
                          const Tex2DParams &params, eTexLoadStatus *load_status) {
    strcpy(name_, name);
    ready_ = false;
    if (load_status) {
        *load_status = TexNotSupported;
    }
    ((void)data);
    ((void)size);
    ((void)params);
}

void Ren::Texture2D::InitFromRAWData(const void *data, const Tex2DParams &p) {
    SWint tex_id;
    if (params_.format == Undefined) {
        tex_id = swCreateTexture();
        tex_id_ = (uint32_t)tex_id;
    } else {
        tex_id = (SWint)tex_id_;
    }

    swActiveTexture(SW_TEXTURE0);
    swBindTexture(tex_id);

    params_ = p;

    if (p.format == RawRGBA8888) {
        swTexImage2D(SW_RGBA, SW_UNSIGNED_BYTE, p.w, p.h, data);
    } else if (p.format == RawRGB888) {
        swTexImage2D(SW_RGB, SW_UNSIGNED_BYTE, p.w, p.h, data);
    }
}

void Ren::Texture2D::InitFromTGAFile(const void *data, const Tex2DParams &p) {
    int w = 0, h = 0;
    eTexFormat format = Undefined;
    auto image_data = ReadTGAFile(data, w, h, format);

    Tex2DParams _p = p;
    _p.w = w;
    _p.h = h;
    _p.format = format;

    InitFromRAWData(image_data.get(), _p);
}

void Ren::Texture2D::InitFromTEXFile(const void *data, const Tex2DParams &p) {
    ((void)p);

    SWint tex_id;
    if (params_.format == Undefined) {
        tex_id = swCreateTexture();
        tex_id_ = (uint32_t)tex_id;
    } else {
        tex_id = (SWint)tex_id_;
    }

    swActiveTexture(SW_TEXTURE0);
    swBindTexture(tex_id);

    if (data) {
        unsigned short *res = (unsigned short *)data;

        params_.format = Compressed;
        params_.w = res[0];
        params_.h = res[1];

        swTexImage2D(SW_RGBA, SW_COMPRESSED, params_.w, params_.h, (char *)data + 4);
    }
}

void Ren::Texture2D::SetFilter(eTexFilter f, eTexRepeat r) {
    ((void)f);
    ((void)r);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif