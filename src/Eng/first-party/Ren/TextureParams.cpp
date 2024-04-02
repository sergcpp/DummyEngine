#include "TextureParams.h"

#include <algorithm>

bool Ren::IsCompressedFormat(const eTexFormat format) {
    switch (format) {
    case eTexFormat::BC1:
    case eTexFormat::BC2:
    case eTexFormat::BC3:
    case eTexFormat::BC4:
    case eTexFormat::BC5:
    case eTexFormat::ASTC:
        return true;
    default:
        return false;
    }
    return false;
}

int Ren::CalcMipCount(const int w, const int h, const int min_res, eTexFilter filter) {
    int mip_count = 0;
    if (filter == eTexFilter::Trilinear || filter == eTexFilter::Bilinear || filter == eTexFilter::NearestMipmap) {
        int max_dim = std::max(w, h);
        do {
            mip_count++;
        } while ((max_dim /= 2) >= min_res);
    } else {
        mip_count = 1;
    }
    return mip_count;
}

int Ren::GetColorChannelCount(const eTexFormat format) {
    static_assert(int(eTexFormat::_Count) == 35, "Update the list below!");
    switch (format) {
    case eTexFormat::RawRGBA8888:
    case eTexFormat::RawRGBA8888Snorm:
    case eTexFormat::RawBGRA8888:
    case eTexFormat::RawRGBA32F:
    case eTexFormat::RawRGBE8888:
    case eTexFormat::RawRGBA16F:
    case eTexFormat::RawRGB10_A2:
    case eTexFormat::BC2:
    case eTexFormat::BC3:
        return 4;
    case eTexFormat::RawRGB888:
    case eTexFormat::RawRGB32F:
    case eTexFormat::RawRGB16F:
    case eTexFormat::RawRG11F_B10F:
    case eTexFormat::RawRGB9E5:
    case eTexFormat::BC1:
        return 3;
    case eTexFormat::RawRG88:
    case eTexFormat::RawRG16:
    case eTexFormat::RawRG16Snorm:
    case eTexFormat::RawRG16F:
    case eTexFormat::RawRG32F:
    case eTexFormat::RawRG32UI:
    case eTexFormat::BC5:
        return 2;
    case eTexFormat::RawR32F:
    case eTexFormat::RawR16F:
    case eTexFormat::RawR8:
    // case eTexFormat::RawR16UI:
    case eTexFormat::RawR32UI:
    case eTexFormat::BC4:
        return 1;
    case eTexFormat::Depth16:
    case eTexFormat::Depth24Stencil8:
    case eTexFormat::Depth32Stencil8:
#ifndef __ANDROID__
    case eTexFormat::Depth32:
#endif
    case eTexFormat::Undefined:
    default:
        return 0;
    }
}