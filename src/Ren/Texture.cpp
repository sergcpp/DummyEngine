#include "Texture.h"

namespace Ren {
static const int g_block_res[][2] = {
    {4, 4},   // _4x4
    {5, 4},   // _5x4
    {5, 5},   // _5x5
    {6, 5},   // _6x5
    {6, 6},   // _6x6
    {8, 5},   // _8x5
    {8, 6},   // _8x6
    {8, 8},   // _8x8
    {10, 5},  // _10x5
    {10, 6},  // _10x6
    {10, 8},  // _10x8
    {10, 10}, // _10x10
    {12, 10}, // _12x10
    {12, 12}  // _12x12
};
static_assert(sizeof(g_block_res) / sizeof(g_block_res[0]) == int(eTexBlock::_None), "!");
} // namespace Ren

bool Ren::IsCompressedFormat(const eTexFormat format) {
    switch (format) {
    case eTexFormat::Compressed_DXT1:
    case eTexFormat::Compressed_DXT3:
    case eTexFormat::Compressed_DXT5:
    case eTexFormat::Compressed_ASTC:
        return true;
    default:
        return false;
    }
    return false;
}

int Ren::CalcMipCount(const int w, const int h, const int min_res, eTexFilter filter) {
    int mip_count = 0;
    if (filter == eTexFilter::Trilinear || filter == eTexFilter::Bilinear) {
        int max_dim = std::max(w, h);
        do {
            mip_count++;
        } while ((max_dim /= 2) >= min_res);
    } else {
        mip_count = 1;
    }
    return mip_count;
}

int Ren::GetBlockLenBytes(const eTexFormat format, const eTexBlock block) {
    switch (format) {
    case eTexFormat::Compressed_DXT1:
        assert(block == eTexBlock::_4x4);
        return 8;
    case eTexFormat::Compressed_DXT3:
    case eTexFormat::Compressed_DXT5:
        assert(block == eTexBlock::_4x4);
        return 16;
    case eTexFormat::Compressed_ASTC:
        assert(false);
    default:
        return -1;
    }
    return -1;
}

int Ren::GetBlockCount(const int w, const int h, const eTexBlock block) {
    const int i = int(block);
    return ((w + g_block_res[i][0] - 1) / g_block_res[i][0]) *
           ((h + g_block_res[i][1] - 1) / g_block_res[i][1]);
}
