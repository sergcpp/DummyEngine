#include "Image.h"

#include "Utils.h"

#if defined(REN_GL_BACKEND)
#include "GL.h"
#endif

namespace Ren {
#define X(_0, _1, _2, _3, _4, _5, _6, _7, _8) {_1, _2, _3, _4},
struct {
    int channel_count;
    int pp_data_len;
    int block_x;
    int block_y;
} g_tex_format_info[] = {
#include "Format.inl"
};
#undef X

const eImgUsage g_img_usage_per_state[] = {
    {},                      // Undefined
    {},                      // Discarded
    {},                      // VertexBuffer
    {},                      // UniformBuffer
    {},                      // IndexBuffer
    eImgUsage::RenderTarget, // RenderTarget
    eImgUsage::Storage,      // UnorderedAccess
    eImgUsage::RenderTarget, // DepthRead
    eImgUsage::RenderTarget, // DepthWrite
    eImgUsage::RenderTarget, // StencilTestDepthFetch
    eImgUsage::Sampled,      // ShaderResource
    {},                      // IndirectArgument
    eImgUsage::Transfer,     // CopyDst
    eImgUsage::Transfer,     // CopySrc
    {},                      // BuildASRead
    {},                      // BuildASWrite
    {}                       // RayTracing
};
static_assert(std::size(g_img_usage_per_state) == int(eResState::_Count));
} // namespace Ren

int Ren::GetBlockLenBytes(const eFormat format) {
    static_assert(int(eFormat::_Count) == 67, "Update the list below!");
    switch (format) {
    case eFormat::BC1:
    case eFormat::BC1_srgb:
        return BlockSize_BC1;
    case eFormat::BC2:
    case eFormat::BC2_srgb:
    case eFormat::BC3:
    case eFormat::BC3_srgb:
        return BlockSize_BC3;
    case eFormat::BC4:
        return BlockSize_BC4;
    case eFormat::BC5:
        return BlockSize_BC5;
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
        assert(false);
        break;
    default:
        return -1;
    }
    return -1;
}

int Ren::GetBlockCount(const int w, const int h, const eFormat format) {
    const int i = int(format);
    return ((w + g_tex_format_info[i].block_x - 1) / g_tex_format_info[i].block_x) *
           ((h + g_tex_format_info[i].block_y - 1) / g_tex_format_info[i].block_y);
}

int Ren::GetDataLenBytes(const int w, const int h, const int d, const eFormat format) {
    if (IsCompressedFormat(format)) {
        const int block_len = GetBlockLenBytes(format);
        const int block_cnt = GetBlockCount(w, h, format);
        return block_len * block_cnt * std::max(d, 1);
    } else {
        assert(g_tex_format_info[int(format)].pp_data_len != 0);
        return w * h * d * g_tex_format_info[int(format)].pp_data_len;
    }
}

uint32_t Ren::GetDataLenBytes(const ImgParams &params) {
    uint32_t total_len = 0;
    for (int i = 0; i < params.mip_count; i++) {
        const int w = std::max(params.w >> i, 1);
        const int h = std::max(params.h >> i, 1);
        const int d = std::max(params.d >> i, 1);

        if (IsCompressedFormat(params.format)) {
            const int block_len = GetBlockLenBytes(params.format);
            const int block_cnt = GetBlockCount(w, h, params.format);

            total_len += uint32_t(block_len) * block_cnt * std::max(int(params.d), 1);
        } else {
            assert(g_tex_format_info[int(params.format)].pp_data_len != 0);
            total_len += w * h * d * g_tex_format_info[int(params.format)].pp_data_len;
        }
    }
    return (params.flags & eImgFlags::CubeMap) ? 6 * total_len : total_len;
}

//
// All this is needed when reading KTX files
//
#if !defined(REN_GL_BACKEND)
[[maybe_unused]] static const uint32_t GL_COMPRESSED_RGB_S3TC_DXT1_EXT = 33776;
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

Ren::eFormat Ren::FormatFromGLInternalFormat(const uint32_t gl_internal_format, bool *is_srgb) {
    (*is_srgb) = false;

    switch (gl_internal_format) {
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return eFormat::BC1;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return eFormat::BC2;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return eFormat::BC3;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        return eFormat::ASTC_4x4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        return eFormat::ASTC_5x4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        return eFormat::ASTC_5x5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        return eFormat::ASTC_6x5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        return eFormat::ASTC_6x6;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        return eFormat::ASTC_8x5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        return eFormat::ASTC_8x6;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        return eFormat::ASTC_8x8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        return eFormat::ASTC_10x5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        return eFormat::ASTC_10x6;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        return eFormat::ASTC_10x8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        return eFormat::ASTC_10x10;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        return eFormat::ASTC_12x10;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        return eFormat::ASTC_12x12;
    default:
        assert(false && "Unsupported format!");
    }

    return eFormat::Undefined;
}

int Ren::BlockLenFromGLInternalFormat(uint32_t gl_internal_format) {
    switch (gl_internal_format) {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return 8;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return 16;
    default:
        assert(false);
    }
    return -1;
}

Ren::eImgUsage Ren::ImgUsageFromState(eResState state) { return g_img_usage_per_state[int(state)]; }

void Ren::ParseDDSHeader(const DDSHeader &hdr, ImgParams *params) {
    params->w = uint16_t(hdr.dwWidth);
    params->h = uint16_t(hdr.dwHeight);
    params->d = uint8_t(hdr.dwDepth > 1 ? hdr.dwDepth : 0);
    params->mip_count = uint8_t(hdr.dwMipMapCount);

    if (hdr.sPixelFormat.dwFourCC == FourCC_BC1) {
        // assume SRGB by default
        params->format = eFormat::BC1_srgb;
    } else if (hdr.sPixelFormat.dwFourCC == FourCC_BC2) {
        params->format = eFormat::BC2;
    } else if (hdr.sPixelFormat.dwFourCC == FourCC_BC3) {
        params->format = eFormat::BC3;
    } else if (hdr.sPixelFormat.dwFourCC == FourCC_BC4) {
        params->format = eFormat::BC4;
    } else if (hdr.sPixelFormat.dwFourCC == FourCC_BC5) {
        params->format = eFormat::BC5;
    } else {
        if (hdr.sPixelFormat.dwFlags & DDPF_RGB) {
            // Uncompressed
            if (hdr.sPixelFormat.dwRGBBitCount == 32) {
                params->format = eFormat::RGBA8;
            } else if (hdr.sPixelFormat.dwRGBBitCount == 24) {
                params->format = eFormat::RGB8;
            } else {
                params->format = eFormat::Undefined;
            }
        } else {
            // Possibly need to read DX10 header
            params->format = eFormat::Undefined;
        }
    }
}