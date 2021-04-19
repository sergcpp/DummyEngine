#pragma once

#include <cstdint>
#undef Always

#include "Fixed.h"

namespace Ren {
class Context;

enum class eTexFormat : uint8_t {
    Undefined,
    RawRGB888,
    RawRGBA8888,
    RawRGBA8888Snorm,
    RawR32F,
    RawR16F,
    RawR8,
    RawRG88,
    RawRGB32F,
    RawRGBA32F,
    RawRGBE8888,
    RawRGB16F,
    RawRGBA16F,
    RawRG16,
    RawRG16U,
    RawRG16F,
    RawRG32F,
    RawRG32U,
    RawRGB10_A2,
    RawRG11F_B10F,
    Depth16,
    Depth24Stencil8,
#ifndef __ANDROID__
    Depth32,
#endif
    Compressed_DXT1,
    Compressed_DXT3,
    Compressed_DXT5,
    Compressed_ASTC,
    None,
    _Count
};

#if defined(__ANDROID__)
const Ren::eTexFormat DefaultCompressedRGBA = Ren::eTexFormat::Compressed_ASTC;
#else
const Ren::eTexFormat DefaultCompressedRGBA = Ren::eTexFormat::Compressed_DXT5;
#endif

enum class eTexBlock : uint8_t {
    _4x4,
    _5x4,
    _5x5,
    _6x5,
    _6x6,
    _8x5,
    _8x6,
    _8x8,
    _10x5,
    _10x6,
    _10x8,
    _10x10,
    _12x10,
    _12x12,
    _None
};

enum class eTexFilter : uint8_t {
    NoFilter,
    Bilinear,
    Trilinear,
    BilinearNoMipmap,
    _Count
};
enum class eTexRepeat : uint8_t { Repeat, ClampToEdge, ClampToBorder, WrapModesCount };

enum class eTexCompare : uint8_t {
    None,
    LEqual,
    GEqual,
    Less,
    Greater,
    Equal,
    NotEqual,
    Always,
    Never,
    _Count
};

using Fixed8 = Fixed<int8_t, 3>;

struct TexSamplingParams {
    eTexFilter filter = eTexFilter::NoFilter;
    eTexRepeat repeat = eTexRepeat::Repeat;
    eTexCompare compare = eTexCompare::None;
    Fixed8 lod_bias;
    Fixed8 min_lod = Fixed8::lowest(), max_lod = Fixed8::max();
};
static_assert(sizeof(TexSamplingParams) == 6, "!");

inline bool operator==(const TexSamplingParams &lhs, const TexSamplingParams &rhs) {
    return lhs.filter == rhs.filter && lhs.repeat == rhs.repeat &&
           lhs.compare == rhs.compare && lhs.lod_bias == rhs.lod_bias &&
           lhs.min_lod == rhs.min_lod && lhs.max_lod == rhs.max_lod;
}

enum class eBindTarget : uint16_t { Tex2D, Tex2DMs, TexCubeArray, TexBuf, UBuf, _Count };

bool IsCompressedFormat(eTexFormat format);
int CalcMipCount(int w, int h, int min_res, eTexFilter filter);
int GetBlockLenBytes(eTexFormat format, eTexBlock block);
int GetBlockCount(int w, int h, eTexBlock block);
inline int GetMipDataLenBytes(const int w, const int h, const eTexFormat format,
                              const eTexBlock block) {
    return GetBlockCount(w, h, block) * GetBlockLenBytes(format, block);
}
} // namespace Ren

#if defined(USE_GL_RENDER)
#include "TextureGL.h"
#elif defined(USE_SW_RENDER)
#include "TextureSW.h"
#endif
