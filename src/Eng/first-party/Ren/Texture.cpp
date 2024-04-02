#include "Texture.h"

#include "Utils.h"

#if defined(USE_GL_RENDER)
#include "GL.h"
#endif

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

const eTexUsage g_tex_usage_per_state[] = {
    {},                      // Undefined
    {},                      // VertexBuffer
    {},                      // UniformBuffer
    {},                      // IndexBuffer
    eTexUsage::RenderTarget, // RenderTarget
    eTexUsage::Storage,      // UnorderedAccess
    eTexUsage::RenderTarget, // DepthRead
    eTexUsage::RenderTarget, // DepthWrite
    eTexUsage::RenderTarget, // StencilTestDepthFetch
    eTexUsage::Sampled,      // ShaderResource
    {},                      // IndirectArgument
    eTexUsage::Transfer,     // CopyDst
    eTexUsage::Transfer,     // CopySrc
    {},                      // BuildASRead
    {},                      // BuildASWrite
    {}                       // RayTracing
};
static_assert(sizeof(g_tex_usage_per_state) / sizeof(g_tex_usage_per_state[0]) == int(eResState::_Count), "!");

const int g_per_pixel_data_len[] = {
    -1, // Undefined
    3,  // RawRGB888
    4,  // RawRGBA8888
    4,  // RawRGBA8888Snorm
    4,  // RawBGRA8888
    4,  // RawR32F
    2,  // RawR16F
    1,  // RawR8
    4,  // RawR32UI
    2,  // RawRG88
    12, // RawRGB32F
    16, // RawRGBA32F
    16, // RawRGBA32UI
    4,  // RawRGBE8888
    6,  // RawRGB16F
    8,  // RawRGBA16F
    4,  // RawRG16Snorm
    4,  // RawRG16
    4,  // RawRG16F
    8,  // RawRG32F
    8,  // RawRG32UI
    4,  // RawRGB10_A2
    4,  // RawRG11F_B10F
    4,  // RawRGB9E5
    2,  // Depth16
    4,  // Depth24Stencil8
    5,  // Depth32Stencil8
#ifndef __ANDROID__
    4, // Depth32
#endif
    -1, // BC1
    -1, // BC2
    -1, // BC3
    -1, // BC4
    -1, // BC5
    -1, // ASTC
    -1  // None
};
static_assert(sizeof(g_per_pixel_data_len) / sizeof(g_per_pixel_data_len[0]) == int(eTexFormat::_Count), "!");
} // namespace Ren

int Ren::GetBlockLenBytes(const eTexFormat format, const eTexBlock block) {
    switch (format) {
    case eTexFormat::BC1:
        assert(block == eTexBlock::_4x4);
        return BlockSize_BC1;
    case eTexFormat::BC2:
    case eTexFormat::BC3:
        assert(block == eTexBlock::_4x4);
        return BlockSize_BC3;
    case eTexFormat::BC4:
        assert(block == eTexBlock::_4x4);
        return BlockSize_BC4;
    case eTexFormat::BC5:
        assert(block == eTexBlock::_4x4);
        return BlockSize_BC5;
    case eTexFormat::ASTC:
        assert(false);
        break;
    default:
        return -1;
    }
    return -1;
}

int Ren::GetBlockCount(const int w, const int h, const eTexBlock block) {
    const int i = int(block);
    return ((w + g_block_res[i][0] - 1) / g_block_res[i][0]) * ((h + g_block_res[i][1] - 1) / g_block_res[i][1]);
}

int Ren::GetMipDataLenBytes(const int w, const int h, const eTexFormat format, const eTexBlock block) {
    if (block != eTexBlock::_None) {
        return GetBlockCount(w, h, block) * GetBlockLenBytes(format, block);
    } else {
        assert(g_per_pixel_data_len[int(format)] != -1);
        return w * h * g_per_pixel_data_len[int(format)];
    }
}

uint32_t Ren::EstimateMemory(const Tex2DParams &params) {
    uint32_t total_len = 0;
    for (int i = 0; i < params.mip_count; i++) {
        const int w = std::max(params.w >> i, 1);
        const int h = std::max(params.h >> i, 1);

        if (IsCompressedFormat(params.format)) {
            const int block_len = GetBlockLenBytes(params.format, params.block);
            const int block_cnt = GetBlockCount(w, h, params.block);

            total_len += uint32_t(block_len) * block_cnt;
        } else {
            assert(g_per_pixel_data_len[int(params.format)] != -1);
            total_len += w * h * g_per_pixel_data_len[int(params.format)];
        }
    }
    return params.cube ? 6 * total_len : total_len;
}

//
// All this is needed when reading KTX files
//
#if !defined(USE_GL_RENDER)
static const uint32_t GL_COMPRESSED_RGB_S3TC_DXT1_EXT = 33776;
static const uint32_t GL_COMPRESSED_RGBA_S3TC_DXT1_EXT = 33777;
static const uint32_t GL_COMPRESSED_RGBA_S3TC_DXT3_EXT = 33778;
static const uint32_t GL_COMPRESSED_RGBA_S3TC_DXT5_EXT = 33779;

static const uint32_t GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT = 35917;
static const uint32_t GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT = 35918;
static const uint32_t GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT = 35919;

static const uint32_t GL_COMPRESSED_RGBA_ASTC_4x4_KHR = 0x93B0;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_5x4_KHR = 0x93B1;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_5x5_KHR = 0x93B2;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_6x5_KHR = 0x93B3;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_6x6_KHR = 0x93B4;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_8x5_KHR = 0x93B5;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_8x6_KHR = 0x93B6;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_8x8_KHR = 0x93B7;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_10x5_KHR = 0x93B8;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_10x6_KHR = 0x93B9;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_10x8_KHR = 0x93BA;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_10x10_KHR = 0x93BB;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_12x10_KHR = 0x93BC;
static const uint32_t GL_COMPRESSED_RGBA_ASTC_12x12_KHR = 0x93BD;

static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR = 0x93D0;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR = 0x93D1;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR = 0x93D2;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR = 0x93D3;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR = 0x93D4;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR = 0x93D5;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR = 0x93D6;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR = 0x93D7;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR = 0x93D8;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR = 0x93D9;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR = 0x93DA;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR = 0x93DB;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR = 0x93DC;
static const uint32_t GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR = 0x93DD;
#endif

Ren::eTexFormat Ren::FormatFromGLInternalFormat(const uint32_t gl_internal_format, eTexBlock *block, bool *is_srgb) {
    (*is_srgb) = false;

    switch (gl_internal_format) {
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        (*block) = eTexBlock::_4x4;
        return eTexFormat::BC1;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        (*block) = eTexBlock::_4x4;
        return eTexFormat::BC2;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        (*block) = eTexBlock::_4x4;
        return eTexFormat::BC3;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        (*block) = eTexBlock::_4x4;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        (*block) = eTexBlock::_5x4;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        (*block) = eTexBlock::_5x5;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        (*block) = eTexBlock::_6x5;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        (*block) = eTexBlock::_6x6;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        (*block) = eTexBlock::_8x5;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        (*block) = eTexBlock::_8x6;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        (*block) = eTexBlock::_8x8;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        (*block) = eTexBlock::_10x5;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        (*block) = eTexBlock::_10x6;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        (*block) = eTexBlock::_10x8;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        (*block) = eTexBlock::_10x10;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        (*block) = eTexBlock::_12x10;
        return eTexFormat::ASTC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        (*block) = eTexBlock::_12x12;
        return eTexFormat::ASTC;
    default:
        assert(false && "Unsupported format!");
    }

    return eTexFormat::Undefined;
}

int Ren::BlockLenFromGLInternalFormat(uint32_t gl_internal_format) {
    switch (gl_internal_format) {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return 8;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return 16;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return 16;
    default:
        assert(false);
    }
    return -1;
}

Ren::eTexUsage Ren::TexUsageFromState(Ren::eResState state) { return g_tex_usage_per_state[int(state)]; }