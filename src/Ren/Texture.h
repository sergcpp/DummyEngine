#pragma once

namespace Ren {
    class Context;

    enum class eTexFormat { Undefined, RawRGB888, RawRGBA8888, RawRGBA8888Snorm, RawR32F, RawR16F, RawR8, RawRG88, RawRGB32F, RawRGBA32F, RawRGBE8888, RawRGB16F, RawRGBA16F, RawRG16, RawRG16U, RawRG16F, RawRG32F, RawRGB10_A2, RawRG11F_B10F, Compressed, None, FormatCount };
    enum class eTexFilter { NoFilter, Bilinear, Trilinear, BilinearNoMipmap, FilterCount };
    enum class eTexRepeat { Repeat, ClampToEdge, ClampToBorder, WrapModesCount };

    int CalcMipCount(int w, int h, int min_res, eTexFilter filter);
}

#if defined(USE_GL_RENDER)
#include "TextureGL.h"
#elif defined(USE_SW_RENDER)
#include "TextureSW.h"
#endif
