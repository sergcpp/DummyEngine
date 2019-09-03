#pragma once

#include <cstdint>
#include <cstring>

#include "Storage.h"
#include "String.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
enum eTexColorFormat { Undefined, RawRGB888, RawRGBA8888, RawLUM8, RawR32F, RawR16F, RawR8, RawRG88, RawRGB32F, RawRGBA32F, RawRGBE8888, RawRGB16F, RawRGBA16F, RawRG16U, RawRG16F, RawRG32F, RawRGB10_A2, RawRG11F_B10F, Compressed, None, FormatCount };
enum eTexFilter { NoFilter, Bilinear, Trilinear, BilinearNoMipmap };
enum eTexRepeat { Repeat, ClampToEdge };

struct Texture2DParams {
    int w = 0, h = 0, cube = 0;
    eTexColorFormat format = Undefined;
    eTexFilter filter = NoFilter;
    eTexRepeat repeat = Repeat;
    uint32_t flags = 0;
};

enum eTexLoadStatus { TexFound, TexCreatedDefault, TexCreatedFromData };

class Texture2D : public RefCounter {
    uint32_t        tex_id_ = 0;
    Texture2DParams params_;
    bool            ready_ = false;
    uint32_t        cubemap_ready_ = 0;
    String          name_;

    void InitFromRAWData(const void *data, const Texture2DParams &p);
    void InitFromTGAFile(const void *data, const Texture2DParams &p);
    void InitFromTGA_RGBEFile(const void *data, const Texture2DParams &p);
    void InitFromDDSFile(const void *data, int size, const Texture2DParams &p);
    void InitFromPNGFile(const void *data, int size, const Texture2DParams &p);
    void InitFromKTXFile(const void *data, int size, const Texture2DParams &p);

    void InitFromRAWData(const void *data[6], const Texture2DParams &p);
    void InitFromTGAFile(const void *data[6], const Texture2DParams &p);
    void InitFromTGA_RGBEFile(const void *data[6], const Texture2DParams &p);
    void InitFromPNGFile(const void *data[6], const int size[6], const Texture2DParams &p);
    void InitFromDDSFile(const void *data[6], const int size[6], const Texture2DParams &p);
    void InitFromKTXFile(const void *data[6], const int size[6], const Texture2DParams &p);

public:
    Texture2D() {}
    Texture2D(const char *name, uint32_t tex_id, const Texture2DParams &params)
        : tex_id_(tex_id), params_(params), ready_(0), name_(name) {        
    }
    Texture2D(const char *name, const void *data, int size, const Texture2DParams &params, eTexLoadStatus *load_status);
    Texture2D(const char *name, const void *data[6], const int size[6], const Texture2DParams &params, eTexLoadStatus *load_status);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) {
        *this = std::move(rhs);
    }
    ~Texture2D();

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs);

    void Init(const void *data, int size, const Texture2DParams &params, eTexLoadStatus *load_status);
    void Init(const void *data[6], const int size[6], const Texture2DParams &params, eTexLoadStatus *load_status);

    uint32_t tex_id() const {
        return tex_id_;
    }
    const Texture2DParams &params() const {
        return params_;
    }
    bool ready() const {
        return ready_;
    }
    const String &name() const {
        return name_;
    }

    void ChangeFilter(eTexFilter f, eTexRepeat r);

    void ReadTextureData(eTexColorFormat format, void *out_data) const;
};

uint32_t GLFormatFromTexFormat(eTexColorFormat format);
uint32_t GLInternalFormatFromTexFormat(eTexColorFormat format);
uint32_t GLTypeFromTexFormat(eTexColorFormat format);

typedef StorageRef<Texture2D> Texture2DRef;
typedef Storage<Texture2D> Texture2DStorage;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif