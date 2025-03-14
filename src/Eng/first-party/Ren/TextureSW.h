#pragma once

#include <cstdint>
#include <cstring>

#include "Storage.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
enum eTexFormat { Undefined, RGB8, RGBA8, Compressed };
enum eTexFilter { NoFilter, Bilinear, Trilinear, BilinearNoMipmap };
enum eTexRepeat { Repeat, ClampToEdge };

struct Tex2DParams {
    int w = 0, h = 0;
    eTexFormat format = Undefined;
    eTexFilter filter = NoFilter;
    eTexRepeat repeat = Repeat;
};

enum eTexLoadStatus { TexFound, TexCreatedDefault, TexCreatedFromData, TexNotSupported };

class Texture2D : public RefCounter {
    uint32_t    tex_id_ = 0;
    Tex2DParams params_;
    bool        ready_ = false;
    char        name_[64];

    void InitFromRAWData(const void *data, const Tex2DParams &p);
    void InitFromTGAFile(const void *data, const Tex2DParams &p);
    void InitFromTEXFile(const void *data, const Tex2DParams &p);
public:
    Texture2D() {
        name_[0] = '\0';
    }
    Texture2D(std::string_view name, uint32_t tex_id, const Tex2DParams &params) : tex_id_(tex_id), params_(params), ready_(0) {
        strcpy(name_, name);
    }
    Texture2D(std::string_view name, const void *data, int size, const Tex2DParams &params, eTexLoadStatus *load_status);
    Texture2D(std::string_view name, const void *data[6], const int size[6], const Tex2DParams &params, eTexLoadStatus *load_status);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) {
        *this = std::move(rhs);
    }
    ~Texture2D();

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs);

    void Init(std::string_view name, const void *data, int size, const Tex2DParams &params, eTexLoadStatus *load_status);
    void Init(std::string_view name, const void *data[6], const int size[6], const Tex2DParams &params, eTexLoadStatus *load_status);

    uint32_t tex_id() const {
        return tex_id_;
    }
    const Tex2DParams &params() const {
        return params_;
    }
    bool ready() const {
        return ready_;
    }
    const char *name() const {
        return name_;
    }

    void SetFilter(eTexFilter f, eTexRepeat r);
};

typedef StrongRef<Texture2D> Tex2DRef;
typedef Storage<Texture2D> Texture2DStorage;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif