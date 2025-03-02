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

enum class eTexFlags : uint8_t {
    NoOwnership,
    Signed,
    SRGB,
    NoRepeat,
    MIPMin,
    MIPMax,
    NoBias,
    ExtendedViews
};

enum class eTexUsage : uint8_t { Transfer, Sampled, Storage, RenderTarget };

struct Texture1DParams {
    uint32_t offset = 0, size = 0;
    uint8_t _padding[3];
    eTexFormat format = eTexFormat::Undefined;
};
static_assert(sizeof(Texture1DParams) == 12, "!");

struct Tex2DParams {
    uint16_t w = 0, h = 0;
    Bitmask<eTexFlags> flags;
    uint8_t mip_count = 1;
    Bitmask<eTexUsage> usage;
    uint8_t cube = 0;
    uint8_t samples = 1;
    uint8_t fallback_color[4] = {0, 0, 0, 255};
    eTexFormat format = eTexFormat::Undefined;
    SamplingParams sampling;
};
static_assert(sizeof(Tex2DParams) == 20, "!");

inline bool operator==(const Tex2DParams &lhs, const Tex2DParams &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.flags == rhs.flags && lhs.mip_count == rhs.mip_count &&
           lhs.usage == rhs.usage && lhs.cube == rhs.cube && lhs.samples == rhs.samples &&
           lhs.fallback_color[0] == rhs.fallback_color[0] && lhs.fallback_color[1] == rhs.fallback_color[1] &&
           lhs.fallback_color[2] == rhs.fallback_color[2] && lhs.fallback_color[3] == rhs.fallback_color[3] &&
           lhs.format == rhs.format && lhs.sampling == rhs.sampling;
}
inline bool operator!=(const Tex2DParams &lhs, const Tex2DParams &rhs) { return !operator==(lhs, rhs); }

struct Tex3DParams {
    uint16_t w = 0, h = 0;
    uint8_t d = 0;
    Bitmask<eTexFlags> flags;
    Bitmask<eTexUsage> usage;
    eTexFormat format = eTexFormat::Undefined;
    SamplingParams sampling;
};
static_assert(sizeof(Tex3DParams) == 14, "!");

inline bool operator==(const Tex3DParams &lhs, const Tex3DParams &rhs) {
    return lhs.w == rhs.w && lhs.h == rhs.h && lhs.d == rhs.d && lhs.flags == rhs.flags && lhs.usage == rhs.usage &&
           lhs.format == rhs.format && lhs.sampling == rhs.sampling;
}
inline bool operator!=(const Tex3DParams &lhs, const Tex3DParams &rhs) { return !operator==(lhs, rhs); }

int GetColorChannelCount(eTexFormat format);

enum class eTexLoadStatus { Found, Reinitialized, CreatedDefault, CreatedFromData, Error };

} // namespace Ren