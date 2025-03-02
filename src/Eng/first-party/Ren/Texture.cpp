#include "Texture.h"

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
#include "TextureFormat.inl"
};
#undef X

const eTexUsage g_tex_usage_per_state[] = {
    {},                      // Undefined
    {},                      // Discarded
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
static_assert(std::size(g_tex_usage_per_state) == int(eResState::_Count), "!");
} // namespace Ren

int Ren::GetBlockLenBytes(const eTexFormat format) {
    switch (format) {
    case eTexFormat::BC1:
        return BlockSize_BC1;
    case eTexFormat::BC2:
    case eTexFormat::BC3:
        return BlockSize_BC3;
    case eTexFormat::BC4:
        return BlockSize_BC4;
    case eTexFormat::BC5:
        return BlockSize_BC5;
    case eTexFormat::ASTC_4x4:
        assert(false);
        break;
    default:
        return -1;
    }
    return -1;
}

int Ren::GetBlockCount(const int w, const int h, const eTexFormat format) {
    const int i = int(format);
    return ((w + g_tex_format_info[i].block_x - 1) / g_tex_format_info[i].block_x) *
           ((h + g_tex_format_info[i].block_y - 1) / g_tex_format_info[i].block_y);
}

int Ren::GetMipDataLenBytes(const int w, const int h, const eTexFormat format) {
    if (IsCompressedFormat(format)) {
        return GetBlockCount(w, h, format) * GetBlockLenBytes(format);
    } else {
        assert(g_tex_format_info[int(format)].pp_data_len != 0);
        return w * h * g_tex_format_info[int(format)].pp_data_len;
    }
}

uint32_t Ren::EstimateMemory(const Tex2DParams &params) {
    uint32_t total_len = 0;
    for (int i = 0; i < params.mip_count; i++) {
        const int w = std::max(params.w >> i, 1);
        const int h = std::max(params.h >> i, 1);

        if (IsCompressedFormat(params.format)) {
            const int block_len = GetBlockLenBytes(params.format);
            const int block_cnt = GetBlockCount(w, h, params.format);

            total_len += uint32_t(block_len) * block_cnt;
        } else {
            assert(g_tex_format_info[int(params.format)].pp_data_len != 0);
            total_len += w * h * g_tex_format_info[int(params.format)].pp_data_len;
        }
    }
    return params.cube ? 6 * total_len : total_len;
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

Ren::eTexFormat Ren::FormatFromGLInternalFormat(const uint32_t gl_internal_format, bool *is_srgb) {
    (*is_srgb) = false;

    switch (gl_internal_format) {
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return eTexFormat::BC1;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return eTexFormat::BC2;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return eTexFormat::BC3;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        return eTexFormat::ASTC_4x4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        return eTexFormat::ASTC_5x4;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        return eTexFormat::ASTC_5x5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        return eTexFormat::ASTC_6x5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        return eTexFormat::ASTC_6x6;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        return eTexFormat::ASTC_8x5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        return eTexFormat::ASTC_8x6;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        return eTexFormat::ASTC_8x8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        return eTexFormat::ASTC_10x5;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        return eTexFormat::ASTC_10x6;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        return eTexFormat::ASTC_10x8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        return eTexFormat::ASTC_10x10;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        return eTexFormat::ASTC_12x10;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        (*is_srgb) = true;
        [[fallthrough]];
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        return eTexFormat::ASTC_12x12;
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
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return 16;
    default:
        assert(false);
    }
    return -1;
}

Ren::eTexUsage Ren::TexUsageFromState(eResState state) { return g_tex_usage_per_state[int(state)]; }

void Ren::ParseDDSHeader(const DDSHeader &hdr, Tex2DParams *params) {
    params->w = uint16_t(hdr.dwWidth);
    params->h = uint16_t(hdr.dwHeight);
    params->mip_count = uint8_t(hdr.dwMipMapCount);

    if (hdr.sPixelFormat.dwFourCC == FourCC_BC1_UNORM) {
        params->format = eTexFormat::BC1;
    } else if (hdr.sPixelFormat.dwFourCC == FourCC_BC2_UNORM) {
        params->format = eTexFormat::BC2;
    } else if (hdr.sPixelFormat.dwFourCC == FourCC_BC3_UNORM) {
        params->format = eTexFormat::BC3;
    } else if (hdr.sPixelFormat.dwFourCC == FourCC_BC4_UNORM) {
        params->format = eTexFormat::BC4;
    } else if (hdr.sPixelFormat.dwFourCC == FourCC_BC5_UNORM) {
        params->format = eTexFormat::BC5;
    } else {
        if (hdr.sPixelFormat.dwFlags & DDPF_RGB) {
            // Uncompressed
            if (hdr.sPixelFormat.dwRGBBitCount == 32) {
                params->format = eTexFormat::RGBA8;
            } else if (hdr.sPixelFormat.dwRGBBitCount == 24) {
                params->format = eTexFormat::RGB8;
            } else {
                params->format = eTexFormat::Undefined;
            }
        } else {
            // Possibly need to read DX10 header
            params->format = eTexFormat::Undefined;
        }
    }
}