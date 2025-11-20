#pragma once

#include <cstdint>
#include <cstring>

#include "Storage.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
enum eFormat { Undefined, RGB8, RGBA8, Compressed };
enum eFilter { NoFilter, Bilinear, Trilinear, BilinearNoMipmap };
enum eTexRepeat { Repeat, ClampToEdge };

struct ImgParams {
    int w = 0, h = 0;
    eFormat format = Undefined;
    eFilter filter = NoFilter;
    eTexRepeat repeat = Repeat;
};

enum eImgLoadStatus { TexFound, TexCreatedDefault, TexCreatedFromData, TexNotSupported };

class Image : public RefCounter {
    uint32_t    tex_id_ = 0;
    ImgParams params_;
    bool        ready_ = false;
    char        name_[64];

    void InitFromRAWData(const void *data, const ImgParams &p);
    void InitFromTGAFile(const void *data, const ImgParams &p);
    void InitFromTEXFile(const void *data, const ImgParams &p);
public:
    Image() {
        name_[0] = '\0';
    }
    Image(std::string_view name, uint32_t tex_id, const ImgParams &params) : tex_id_(tex_id), params_(params), ready_(0) {
        strcpy(name_, name);
    }
    Image(std::string_view name, const void *data, int size, const ImgParams &params, eImgLoadStatus *load_status);
    Image(std::string_view name, const void *data[6], const int size[6], const ImgParams &params, eImgLoadStatus *load_status);
    Image(const Image &rhs) = delete;
    Image(Image &&rhs) {
        *this = std::move(rhs);
    }
    ~Image();

    Image &operator=(const Image &rhs) = delete;
    Image &operator=(Image &&rhs);

    void Init(std::string_view name, const void *data, int size, const ImgParams &params, eImgLoadStatus *load_status);
    void Init(std::string_view name, const void *data[6], const int size[6], const ImgParams &params, eImgLoadStatus *load_status);

    uint32_t tex_id() const {
        return tex_id_;
    }
    const ImgParams &params() const {
        return params_;
    }
    bool ready() const {
        return ready_;
    }
    const char *name() const {
        return name_;
    }

    void SetFilter(eFilter f, eTexRepeat r);
};

typedef StrongRef<Image> ImgRef;
typedef Storage<Image> ImageStorage;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif