#pragma once

#include <cstdint>

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
    Compressed,
    None,
    _Count
};
enum class eTexFilter : uint8_t {
    NoFilter,
    Bilinear,
    Trilinear,
    BilinearNoMipmap,
    _Count
};
enum class eTexRepeat : uint8_t { Repeat, ClampToEdge, ClampToBorder, WrapModesCount };

enum class eTexCompare : uint8_t { None, LEqual, GEqual, Less, Greater, Equal, NotEqual, Always, Never, _Count
};

enum class eBindTarget : uint16_t { Tex2D, Tex2DMs, TexCubeArray, TexBuf, UBuf, _Count };

int CalcMipCount(int w, int h, int min_res, eTexFilter filter);
} // namespace Ren

#if defined(USE_GL_RENDER)
#include "TextureGL.h"
#elif defined(USE_SW_RENDER)
#include "TextureSW.h"
#endif
