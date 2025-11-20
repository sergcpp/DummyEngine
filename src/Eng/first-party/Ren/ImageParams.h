#pragma once

#include <cstdint>
#include <string_view>

#include "Bitmask.h"
#include "SamplingParams.h"

namespace Ren {
#define X(_0, ...) _0,
enum class eFormat : uint8_t {
#include "Format.inl"
    _Count
};
#undef X

inline bool operator<(const eFormat lhs, const eFormat rhs) { return uint8_t(lhs) < uint8_t(rhs); }

std::string_view FormatName(eFormat format);
eFormat Format(std::string_view name);

inline bool IsDepthFormat(const eFormat format) {
    return format == eFormat::D16 || format == eFormat::D24_S8 || format == eFormat::D32_S8 || format == eFormat::D32;
}

inline bool IsDepthStencilFormat(const eFormat format) {
    return format == eFormat::D24_S8 || format == eFormat::D32_S8;
}

inline bool IsUnsignedIntegerFormat(const eFormat format) {
    return format == eFormat::R32UI || format == eFormat::RG32UI || format == eFormat::RGBA32UI;
}

bool IsCompressedFormat(const eFormat format);

int CalcMipCount(int w, int h, int min_res);

#if defined(__ANDROID__)
const eFormat DefaultCompressedRGBA = eFormat::ASTC;
#else
const eFormat DefaultCompressedRGBA = eFormat::BC3;
#endif

enum class eImgFlags : uint8_t { NoOwnership, Stub, CubeMap, Array };

enum class eImgUsage : uint8_t { Transfer, Sampled, Storage, RenderTarget };

struct ImgParams {
    uint16_t w = 0, h = 0;
    uint8_t d = 0, _unused = 0;
    uint8_t mip_count : 5;
    uint8_t samples : 3;
    Bitmask<eImgFlags> flags;
    Bitmask<eImgUsage> usage;
    eFormat format = eFormat::Undefined;
    SamplingParams sampling;

    ImgParams() : mip_count(1), samples(1) {
        assert(mip_count < 32);
        assert(samples < 8);
    }
    ImgParams(const uint16_t _w, const uint16_t _h, const uint8_t _d, const uint8_t _mip_count, const uint8_t _samples,
              const Bitmask<eImgFlags> _flags, const Bitmask<eImgUsage> _usage, const eFormat _format,
              const SamplingParams _sampling)
        : w(_w), h(_h), d(_d), mip_count(_mip_count), samples(_samples), flags(_flags), usage(_usage), format(_format),
          sampling(_sampling) {}
};
static_assert(sizeof(ImgParams) == 14);

inline bool operator==(const ImgParams &lhs, const ImgParams &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.d == rhs.d && lhs.mip_count == rhs.mip_count &&
           lhs.samples == rhs.samples && lhs.flags == rhs.flags && lhs.usage == rhs.usage && lhs.format == rhs.format &&
           lhs.sampling == rhs.sampling;
}
inline bool operator!=(const ImgParams &lhs, const ImgParams &rhs) { return !operator==(lhs, rhs); }

struct ImgParamsPacked {
    uint16_t w = 0, h = 0;
    uint8_t d = 0;
    uint8_t mip_count : 5;
    uint8_t samples : 3;
    uint8_t flags : 4;
    uint8_t usage : 4;
    eFormat format = eFormat::Undefined;
    SamplingParamsPacked sampling;

    ImgParamsPacked() : ImgParamsPacked(ImgParams{}) {}
    ImgParamsPacked(const ImgParams &p)
        : w(p.w), h(p.h), d(p.d), mip_count(p.mip_count), samples(p.samples), flags(p.flags), usage(p.usage),
          format(p.format), sampling(p.sampling) {
        assert(uint8_t(p.flags) < 16);
        assert(uint8_t(p.usage) < 16);
    }

    operator ImgParams() const {
        return ImgParams(w, h, d, mip_count, samples, Bitmask<eImgFlags>(flags), Bitmask<eImgUsage>(usage), format,
                         SamplingParams{sampling});
    }
};
static_assert(sizeof(ImgParamsPacked) == 10);

inline bool operator==(const ImgParamsPacked &lhs, const ImgParamsPacked &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.d == rhs.d && lhs.mip_count == rhs.mip_count &&
           lhs.samples == rhs.samples && lhs.flags == rhs.flags && lhs.usage == rhs.usage && lhs.format == rhs.format &&
           lhs.sampling == rhs.sampling;
}
inline bool operator!=(const ImgParamsPacked &lhs, const ImgParamsPacked &rhs) { return !operator==(lhs, rhs); }

inline bool operator==(const ImgParamsPacked &lhs, const ImgParams &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.d == rhs.d && lhs.mip_count == rhs.mip_count &&
           lhs.samples == rhs.samples && lhs.flags == uint8_t(rhs.flags) && lhs.usage == uint8_t(rhs.usage) &&
           lhs.format == rhs.format && lhs.sampling == rhs.sampling;
}
inline bool operator!=(const ImgParamsPacked &lhs, const ImgParams &rhs) { return !operator==(lhs, rhs); }

int GetColorChannelCount(eFormat format);

enum class eImgLoadStatus { Found, Reinitialized, CreatedDefault, CreatedFromData, Error };

union ClearColor {
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
};

} // namespace Ren