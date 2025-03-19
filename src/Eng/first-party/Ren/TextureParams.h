#pragma once

#include <cstdint>
#include <string_view>

#include "Bitmask.h"
#include "SamplingParams.h"

namespace Ren {
#define X(_0, ...) _0,
enum class eTexFormat : uint8_t {
#include "TextureFormat.inl"
    _Count
};
#undef X

inline bool operator<(const eTexFormat lhs, const eTexFormat rhs) { return uint8_t(lhs) < uint8_t(rhs); }

std::string_view TexFormatName(eTexFormat format);
eTexFormat TexFormat(std::string_view name);

inline bool IsDepthFormat(const eTexFormat format) {
    return format == eTexFormat::D16 || format == eTexFormat::D24_S8 || format == eTexFormat::D32_S8 ||
           format == eTexFormat::D32;
}

inline bool IsDepthStencilFormat(const eTexFormat format) {
    return format == eTexFormat::D24_S8 || format == eTexFormat::D32_S8;
}

inline bool IsUnsignedIntegerFormat(const eTexFormat format) {
    return format == eTexFormat::R32UI || format == eTexFormat::RG32UI || format == eTexFormat::RGBA32UI;
}

bool IsCompressedFormat(const eTexFormat format);

int CalcMipCount(int w, int h, int min_res);

#if defined(__ANDROID__)
const eTexFormat DefaultCompressedRGBA = eTexFormat::ASTC;
#else
const eTexFormat DefaultCompressedRGBA = eTexFormat::BC3;
#endif

enum class eTexFlags : uint8_t { NoOwnership, ExtendedViews, Stub, CubeMap };

enum class eTexUsage : uint8_t { Transfer, Sampled, Storage, RenderTarget };

struct TexParams {
    uint16_t w = 0, h = 0;
    uint8_t d = 0, layer_count = 0;
    uint8_t mip_count : 5;
    uint8_t samples : 3;
    Bitmask<eTexFlags> flags;
    Bitmask<eTexUsage> usage;
    eTexFormat format = eTexFormat::Undefined;
    SamplingParams sampling;

    TexParams() : mip_count(1), samples(1) {
        assert(mip_count < 32);
        assert(samples < 8);
    }
    TexParams(const uint16_t _w, const uint16_t _h, const uint8_t _d, const uint8_t _layer_count,
              const uint8_t _mip_count, const uint8_t _samples, const Bitmask<eTexFlags> _flags,
              const Bitmask<eTexUsage> _usage, const eTexFormat _format, const SamplingParams _sampling)
        : w(_w), h(_h), d(_d), layer_count(_layer_count), mip_count(_mip_count), samples(_samples), flags(_flags),
          usage(_usage), format(_format), sampling(_sampling) {}
};
static_assert(sizeof(TexParams) == 14, "!");

inline bool operator==(const TexParams &lhs, const TexParams &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.d == rhs.d && lhs.layer_count == rhs.layer_count &&
           lhs.mip_count == rhs.mip_count && lhs.samples == rhs.samples && lhs.flags == rhs.flags &&
           lhs.usage == rhs.usage && lhs.format == rhs.format && lhs.sampling == rhs.sampling;
}
inline bool operator!=(const TexParams &lhs, const TexParams &rhs) { return !operator==(lhs, rhs); }

struct TexParamsPacked {
    uint16_t w = 0, h = 0;
    uint8_t d = 0, layer_count;
    uint8_t mip_count : 5;
    uint8_t samples : 3;
    uint8_t flags : 4;
    uint8_t usage : 4;
    eTexFormat format = eTexFormat::Undefined;
    SamplingParamsPacked sampling;

    TexParamsPacked() : TexParamsPacked(TexParams{}) {}
    TexParamsPacked(const TexParams &p)
        : w(p.w), h(p.h), d(p.d), layer_count(p.layer_count), mip_count(p.mip_count), samples(p.samples),
          flags(p.flags), usage(p.usage), format(p.format), sampling(p.sampling) {
        assert(uint8_t(p.flags) < 16);
        assert(uint8_t(p.usage) < 16);
    }

    operator TexParams() const {
        return TexParams(w, h, d, layer_count, mip_count, samples, Bitmask<eTexFlags>(flags), Bitmask<eTexUsage>(usage),
                         format, SamplingParams{sampling});
    }
};
static_assert(sizeof(TexParamsPacked) == 12, "!");

inline bool operator==(const TexParamsPacked &lhs, const TexParamsPacked &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.d == rhs.d && lhs.layer_count == rhs.layer_count &&
           lhs.mip_count == rhs.mip_count && lhs.samples == rhs.samples && lhs.flags == rhs.flags &&
           lhs.usage == rhs.usage && lhs.format == rhs.format && lhs.sampling == rhs.sampling;
}
inline bool operator!=(const TexParamsPacked &lhs, const TexParamsPacked &rhs) { return !operator==(lhs, rhs); }

inline bool operator==(const TexParamsPacked &lhs, const TexParams &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.d == rhs.d && lhs.layer_count == rhs.layer_count &&
           lhs.mip_count == rhs.mip_count && lhs.samples == rhs.samples && lhs.flags == uint8_t(rhs.flags) &&
           lhs.usage == uint8_t(rhs.usage) && lhs.format == rhs.format && lhs.sampling == rhs.sampling;
}
inline bool operator!=(const TexParamsPacked &lhs, const TexParams &rhs) { return !operator==(lhs, rhs); }

int GetColorChannelCount(eTexFormat format);

enum class eTexLoadStatus { Found, Reinitialized, CreatedDefault, CreatedFromData, Error };

} // namespace Ren