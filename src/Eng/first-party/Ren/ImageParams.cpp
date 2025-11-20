#include "ImageParams.h"

#include <algorithm>

namespace Ren {
#define X(_0, _1, _2, _3, _4, _5, _6, _7, _8) #_0, _1,
static const struct {
    std::string_view name;
    int channel_count;
} g_format_props[] = {
#include "Format.inl"
};
#undef X
} // namespace Ren

std::string_view Ren::FormatName(const eFormat format) { return g_format_props[uint8_t(format)].name; }

Ren::eFormat Ren::Format(std::string_view name) {
    for (int i = 0; i < int(eFormat::_Count); ++i) {
        if (name == g_format_props[i].name) {
            return eFormat(i);
        }
    }
    return eFormat::Undefined;
}

bool Ren::IsCompressedFormat(const eFormat format) {
    static_assert(int(eFormat::_Count) == 67, "Update the list below!");
    switch (format) {
    case eFormat::BC1:
    case eFormat::BC1_srgb:
    case eFormat::BC2:
    case eFormat::BC2_srgb:
    case eFormat::BC3:
    case eFormat::BC3_srgb:
    case eFormat::BC4:
    case eFormat::BC5:
    case eFormat::ASTC_4x4:
    case eFormat::ASTC_4x4_srgb:
    case eFormat::ASTC_5x4:
    case eFormat::ASTC_5x4_srgb:
    case eFormat::ASTC_5x5:
    case eFormat::ASTC_5x5_srgb:
    case eFormat::ASTC_6x5:
    case eFormat::ASTC_6x5_srgb:
    case eFormat::ASTC_6x6:
    case eFormat::ASTC_6x6_srgb:
    case eFormat::ASTC_8x5:
    case eFormat::ASTC_8x5_srgb:
    case eFormat::ASTC_8x6:
    case eFormat::ASTC_8x6_srgb:
    case eFormat::ASTC_8x8:
    case eFormat::ASTC_8x8_srgb:
    case eFormat::ASTC_10x5:
    case eFormat::ASTC_10x5_srgb:
    case eFormat::ASTC_10x6:
    case eFormat::ASTC_10x6_srgb:
    case eFormat::ASTC_10x8:
    case eFormat::ASTC_10x8_srgb:
    case eFormat::ASTC_10x10:
    case eFormat::ASTC_10x10_srgb:
    case eFormat::ASTC_12x10:
    case eFormat::ASTC_12x10_srgb:
    case eFormat::ASTC_12x12:
    case eFormat::ASTC_12x12_srgb:
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

int Ren::GetColorChannelCount(const eFormat format) { return g_format_props[uint8_t(format)].channel_count; }