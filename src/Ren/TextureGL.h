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
class ILog;

enum class eTexFormat { Undefined, RawRGB888, RawRGBA8888, RawRGBA8888Snorm, RawR32F, RawR16F, RawR8, RawRG88, RawRGB32F, RawRGBA32F, RawRGBE8888, RawRGB16F, RawRGBA16F, RawRG16, RawRG16U, RawRG16F, RawRG32F, RawRGB10_A2, RawRG11F_B10F, Compressed, None, FormatCount };
enum class eTexFilter { NoFilter, Bilinear, Trilinear, BilinearNoMipmap, FilterCount };
enum class eTexRepeat { Repeat, ClampToEdge, ClampToBorder, WrapModesCount };

enum eTexFlags {
    TexNoOwnership = (1u << 0u),
    TexSigned      = (1u << 1u),
    TexSRGB        = (1u << 2u),
    TexNoRepeat    = (1u << 3u),
    TexUsageScene  = (1u << 4u),
    TexUsageUI     = (1u << 5u)
};

struct Texture2DParams {
    int         w = 0, h = 0, cube = 0;
    eTexFormat  format = eTexFormat::Undefined;
    eTexFilter  filter = eTexFilter::NoFilter;
    eTexRepeat  repeat = eTexRepeat::Repeat;
    float       lod_bias = 0.0f;
    uint32_t    flags = 0;
    uint8_t     fallback_color[4] = { 0, 255, 255, 255 };
};

enum class eTexLoadStatus { TexFound, TexCreatedDefault, TexCreatedFromData };

class Texture2D : public RefCounter {
    uint32_t        tex_id_ = 0;
    Texture2DParams params_;
    bool            ready_ = false;
    uint32_t        cubemap_ready_ = 0;
    String          name_;

    void Free();

    void InitFromRAWData(const void *data, const Texture2DParams &p, ILog *log);
    void InitFromTGAFile(const void *data, const Texture2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data, const Texture2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data, int size, const Texture2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data, int size, const Texture2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data, int size, const Texture2DParams &p, ILog *log);

    void InitFromRAWData(const void *data[6], const Texture2DParams &p, ILog *log);
    void InitFromTGAFile(const void *data[6], const Texture2DParams &p, ILog *log);
    void InitFromTGA_RGBEFile(const void *data[6], const Texture2DParams &p, ILog *log);
    void InitFromPNGFile(const void *data[6], const int size[6], const Texture2DParams &p, ILog *log);
    void InitFromDDSFile(const void *data[6], const int size[6], const Texture2DParams &p, ILog *log);
    void InitFromKTXFile(const void *data[6], const int size[6], const Texture2DParams &p, ILog *log);

public:
    Texture2D() = default;
    Texture2D(const char *name, uint32_t tex_id, const Texture2DParams &params, ILog *log)
        : tex_id_(tex_id), params_(params), ready_(true), name_(name) {
    }
    Texture2D(const char *name, const void *data, int size, const Texture2DParams &params, eTexLoadStatus *load_status, ILog *log);
    Texture2D(const char *name, const void *data[6], const int size[6], const Texture2DParams &params,
            eTexLoadStatus *load_status, ILog *log);
    Texture2D(const Texture2D &rhs) = delete;
    Texture2D(Texture2D &&rhs) noexcept {
        *this = std::move(rhs);
    }
    ~Texture2D();

    Texture2D &operator=(const Texture2D &rhs) = delete;
    Texture2D &operator=(Texture2D &&rhs) noexcept ;

    void Init(const void *data, int size, const Texture2DParams &params, eTexLoadStatus *load_status, ILog *log);
    void Init(const void *data[6], const int size[6], const Texture2DParams &params, eTexLoadStatus *load_status, ILog *log);

    uint32_t tex_id() const {
        return tex_id_;
    }
    const Texture2DParams &params() const {
        return params_;
    }
    Texture2DParams &params() {
        return params_;
    }
    bool ready() const {
        return ready_;
    }
    const String &name() const {
        return name_;
    }

    void SetFilter(eTexFilter f, eTexRepeat r, float lod_bias);

    void ReadTextureData(eTexFormat format, void *out_data) const;
};

uint32_t GLFormatFromTexFormat(eTexFormat format);
uint32_t GLInternalFormatFromTexFormat(eTexFormat format);
uint32_t GLTypeFromTexFormat(eTexFormat format);

typedef StorageRef<Texture2D> Texture2DRef;
typedef Storage<Texture2D> Texture2DStorage;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
