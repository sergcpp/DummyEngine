#include "TextureParams.h"

#include <algorithm>

namespace Ren {
#define X(_0, _1, _2, _3, _4, _5, _6, _7, _8) #_0, _1,
static const struct {
    std::string_view name;
    int channel_count;
} g_format_props[] = {
#include "TextureFormat.inl"
};
#undef X
} // namespace Ren

std::string_view Ren::TexFormatName(const eTexFormat format) { return g_format_props[uint8_t(format)].name; }

Ren::eTexFormat Ren::TexFormat(std::string_view name) {
    for (int i = 0; i < int(eTexFormat::_Count); ++i) {
        if (name == g_format_props[i].name) {
            return eTexFormat(i);
        }
    }
    return eTexFormat::Undefined;
}

bool Ren::IsCompressedFormat(const eTexFormat format) {
    static_assert(int(eTexFormat::_Count) == 66, "Update the list below!");
    switch (format) {
    case eTexFormat::BC1:
    case eTexFormat::BC1_srgb:
    case eTexFormat::BC2:
    case eTexFormat::BC2_srgb:
    case eTexFormat::BC3:
    case eTexFormat::BC3_srgb:
    case eTexFormat::BC4:
    case eTexFormat::BC5:
    case eTexFormat::ASTC_4x4:
    case eTexFormat::ASTC_4x4_srgb:
    case eTexFormat::ASTC_5x4:
    case eTexFormat::ASTC_5x4_srgb:
    case eTexFormat::ASTC_5x5:
    case eTexFormat::ASTC_5x5_srgb:
    case eTexFormat::ASTC_6x5:
    case eTexFormat::ASTC_6x5_srgb:
    case eTexFormat::ASTC_6x6:
    case eTexFormat::ASTC_6x6_srgb:
    case eTexFormat::ASTC_8x5:
    case eTexFormat::ASTC_8x5_srgb:
    case eTexFormat::ASTC_8x6:
    case eTexFormat::ASTC_8x6_srgb:
    case eTexFormat::ASTC_8x8:
    case eTexFormat::ASTC_8x8_srgb:
    case eTexFormat::ASTC_10x5:
    case eTexFormat::ASTC_10x5_srgb:
    case eTexFormat::ASTC_10x6:
    case eTexFormat::ASTC_10x6_srgb:
    case eTexFormat::ASTC_10x8:
    case eTexFormat::ASTC_10x8_srgb:
    case eTexFormat::ASTC_10x10:
    case eTexFormat::ASTC_10x10_srgb:
    case eTexFormat::ASTC_12x10:
    case eTexFormat::ASTC_12x10_srgb:
    case eTexFormat::ASTC_12x12:
    case eTexFormat::ASTC_12x12_srgb:
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

int Ren::GetColorChannelCount(const eTexFormat format) { return g_format_props[uint8_t(format)].channel_count; }