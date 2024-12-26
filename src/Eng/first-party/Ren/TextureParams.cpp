#include "TextureParams.h"

#include <algorithm>

namespace Ren {
#define DECORATE(X, Y, Z, W, XX, YY, ZZ) #X,
static const std::string_view g_format_names[] = {
#include "TextureFormat.inl"
};
#undef DECORATE
} // namespace Ren

std::string_view Ren::TexFormatName(const eTexFormat format) { return g_format_names[uint8_t(format)]; }

Ren::eTexFormat Ren::TexFormat(std::string_view name) {
    for (int i = 0; i < int(eTexFormat::_Count); ++i) {
        if (name == g_format_names[i]) {
            return eTexFormat(i);
        }
    }
    return eTexFormat::Undefined;
}

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

int Ren::CalcMipCount(const int w, const int h, const int min_res) {
    int mip_count = 0;
    int max_dim = std::max(w, h);
    do {
        mip_count++;
    } while ((max_dim /= 2) >= min_res);
    return mip_count;
}

int Ren::GetColorChannelCount(const eTexFormat format) {
    static_assert(int(eTexFormat::_Count) == 33, "Update the list below!");
    switch (format) {
    case eTexFormat::RGBA8:
    case eTexFormat::RGBA8_snorm:
    case eTexFormat::BGRA8:
    case eTexFormat::RGBA32F:
    case eTexFormat::RGBA16F:
    case eTexFormat::RGB10_A2:
    case eTexFormat::BC2:
    case eTexFormat::BC3:
        return 4;
    case eTexFormat::RGB8:
    case eTexFormat::RGB32F:
    case eTexFormat::RGB16F:
    case eTexFormat::RG11F_B10F:
    case eTexFormat::RGB9_E5:
    case eTexFormat::BC1:
        return 3;
    case eTexFormat::RG8:
    case eTexFormat::RG16:
    case eTexFormat::RG16_snorm:
    case eTexFormat::RG16F:
    case eTexFormat::RG32F:
    case eTexFormat::RG32UI:
    case eTexFormat::BC5:
        return 2;
    case eTexFormat::R32F:
    case eTexFormat::R16F:
    case eTexFormat::R8:
    // case eTexFormat::RawR16UI:
    case eTexFormat::R32UI:
    case eTexFormat::BC4:
        return 1;
    case eTexFormat::D16:
    case eTexFormat::D24_S8:
    case eTexFormat::D32_S8:
    case eTexFormat::D32:
    case eTexFormat::Undefined:
    default:
        return 0;
    }
}