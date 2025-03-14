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

struct TexParams {
    int w = 0, h = 0;
    eTexFormat format = Undefined;
    eTexFilter filter = NoFilter;
    eTexRepeat repeat = Repeat;
};

enum eTexLoadStatus { TexFound, TexCreatedDefault, TexCreatedFromData, TexNotSupported };

class Texture : public RefCounter {
    uint32_t    tex_id_ = 0;
    TexParams params_;
    bool        ready_ = false;
    char        name_[64];

    void InitFromRAWData(const void *data, const TexParams &p);
    void InitFromTGAFile(const void *data, const TexParams &p);
    void InitFromTEXFile(const void *data, const TexParams &p);
public:
    Texture() {
        name_[0] = '\0';
    }
    Texture(std::string_view name, uint32_t tex_id, const TexParams &params) : tex_id_(tex_id), params_(params), ready_(0) {
        strcpy(name_, name);
    }
    Texture(std::string_view name, const void *data, int size, const TexParams &params, eTexLoadStatus *load_status);
    Texture(std::string_view name, const void *data[6], const int size[6], const TexParams &params, eTexLoadStatus *load_status);
    Texture(const Texture &rhs) = delete;
    Texture(Texture &&rhs) {
        *this = std::move(rhs);
    }
    ~Texture();

    Texture &operator=(const Texture &rhs) = delete;
    Texture &operator=(Texture &&rhs);

    void Init(std::string_view name, const void *data, int size, const TexParams &params, eTexLoadStatus *load_status);
    void Init(std::string_view name, const void *data[6], const int size[6], const TexParams &params, eTexLoadStatus *load_status);

    uint32_t tex_id() const {
        return tex_id_;
    }
    const TexParams &params() const {
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

typedef StrongRef<Texture> TexRef;
typedef Storage<Texture> TextureStorage;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif