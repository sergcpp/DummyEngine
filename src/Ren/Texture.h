#pragma once

#include <cstdint>
#undef Always

#include "Sampler.h"

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

enum class eBindTarget : uint16_t { Tex2D, Tex2DMs, TexCubeArray, TexBuf, UBuf, _Count };

bool IsCompressedFormat(eTexFormat format);
int CalcMipCount(int w, int h, int min_res, eTexFilter filter);
int GetBlockLenBytes(eTexFormat format, eTexBlock block);
int GetBlockCount(int w, int h, eTexBlock block);
inline int GetMipDataLenBytes(const int w, const int h, const eTexFormat format,
                              const eTexBlock block) {
    return GetBlockCount(w, h, block) * GetBlockLenBytes(format, block);
}

enum class eTexLoadStatus {
    Found,
    Reinitialized,
    CreatedDefault,
    CreatedFromData
};
} // namespace Ren

#if defined(USE_GL_RENDER)
#include "TextureGL.h"
#elif defined(USE_SW_RENDER)
#include "TextureSW.h"
#endif

namespace Ren {
using Tex2DRef = StrongRef<Texture2D>;
using WeakTex2DRef = WeakRef<Texture2D>;
using Texture2DStorage = Storage<Texture2D>;

using Tex1DRef = StrongRef<Texture1D>;
using WeakTex1DRef = WeakRef<Texture1D>;
using Texture1DStorage = Storage<Texture1D>;
} // namespace Ren
