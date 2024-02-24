#pragma once

#include <memory>
#include <vector>

#include "MVec.h"

namespace Ren {
enum class eTexFormat : uint8_t;
std::unique_ptr<uint8_t[]> ReadTGAFile(const void *data, int &w, int &h, eTexFormat &format);
bool ReadTGAFile(const void *data, int &w, int &h, eTexFormat &format, uint8_t *out_data, uint32_t &out_size);

void RGBMDecode(const uint8_t rgbm[4], float out_rgb[3]);
void RGBMEncode(const float rgb[3], uint8_t out_rgbm[4]);

std::unique_ptr<float[]> ConvertRGBE_to_RGB32F(const uint8_t image_data[], int w, int h);
std::unique_ptr<uint16_t[]> ConvertRGBE_to_RGB16F(const uint8_t image_data[], int w, int h);
void ConvertRGBE_to_RGB16F(const uint8_t image_data[], int w, int h, uint16_t *out_data);

std::unique_ptr<uint8_t[]> ConvertRGB32F_to_RGBE(const float image_data[], int w, int h, int channels);
std::unique_ptr<uint8_t[]> ConvertRGB32F_to_RGBM(const float image_data[], int w, int h, int channels);

// Perfectly reversible conversion between RGB and YCoCg (breaks bilinear filtering)
void ConvertRGB_to_YCoCg_rev(const uint8_t in_RGB[3], uint8_t out_YCoCg[3]);
void ConvertYCoCg_to_RGB_rev(const uint8_t in_YCoCg[3], uint8_t out_RGB[3]);

std::unique_ptr<uint8_t[]> ConvertRGB_to_CoCgxY_rev(const uint8_t image_data[], int w, int h);
std::unique_ptr<uint8_t[]> ConvertCoCgxY_to_RGB_rev(const uint8_t image_data[], int w, int h);

// Not-so-perfectly reversible conversion between RGB and YCoCg
void ConvertRGB_to_YCoCg(const uint8_t in_RGB[3], uint8_t out_YCoCg[3]);
void ConvertYCoCg_to_RGB(const uint8_t in_YCoCg[3], uint8_t out_RGB[3]);

std::unique_ptr<uint8_t[]> ConvertRGB_to_CoCgxY(const uint8_t image_data[], int w, int h);
std::unique_ptr<uint8_t[]> ConvertCoCgxY_to_RGB(const uint8_t image_data[], int w, int h);

enum class eMipOp {
    Skip = 0,
    Zero,        // fill with zeroes
    Avg,         // average value of 4 pixels
    Min,         // min value of 4 pixels
    Max,         // max value of 4 pixels
    MinBilinear, // min value of 4 pixels and result of bilinear interpolation with
                 // neighbours
    MaxBilinear  // max value of 4 pixels and result of bilinear interpolation with
                 // neighbours
};
int InitMipMaps(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16], int heights[16], int channels,
                const eMipOp op[4], int min_tex_dim = 1);
int InitMipMapsRGBM(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16], int heights[16], int min_tex_dim = 1);

void ReorderTriangleIndices(const uint32_t *indices, uint32_t indices_count, uint32_t vtx_count, uint32_t *out_indices);

struct vertex_t {
    float p[3], n[3], b[3], t[2][2];
    int index;
};

uint16_t f32_to_f16(float value);

inline int16_t f32_to_s16(const float value) { return int16_t(value * 32767); }
inline uint16_t f32_to_u16(const float value) { return uint16_t(value * 65535); }

void ComputeTangentBasis(std::vector<vertex_t> &vertices, std::vector<uint32_t> index_groups[], int groups_count);

struct KTXHeader { // NOLINT
    char identifier[12] = {'\xAB', 'K', 'T', 'X', ' ', '1', '1', '\xBB', '\r', '\n', '\x1A', '\n'};
    uint32_t endianness = 0x04030201;
    uint32_t gl_type;
    uint32_t gl_type_size;
    uint32_t gl_format;
    uint32_t gl_internal_format;
    uint32_t gl_base_internal_format;
    uint32_t pixel_width;
    uint32_t pixel_height;
    uint32_t pixel_depth;
    uint32_t array_elements_count;
    uint32_t faces_count;
    uint32_t mipmap_levels_count;
    uint32_t key_value_data_size;
};
static_assert(sizeof(KTXHeader) == 64, "!");

/*	the following constants were copied directly off the MSDN website	*/

/*	The dwFlags member of the original DDSURFACEDESC2 structure
        can be set to one or more of the following values.	*/
#define DDSD_CAPS 0x00000001
#define DDSD_HEIGHT 0x00000002
#define DDSD_WIDTH 0x00000004
#define DDSD_PITCH 0x00000008
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_MIPMAPCOUNT 0x00020000
#define DDSD_LINEARSIZE 0x00080000
#define DDSD_DEPTH 0x00800000

/*	DirectDraw Pixel Format	*/
#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_FOURCC 0x00000004
#define DDPF_RGB 0x00000040

/*	The dwCaps1 member of the DDSCAPS2 structure can be
        set to one or more of the following values.	*/
#define DDSCAPS_COMPLEX 0x00000008
#define DDSCAPS_TEXTURE 0x00001000
#define DDSCAPS_MIPMAP 0x00400000

const uint32_t FourCC_BC1_UNORM =
    (uint32_t('D') << 0u) | (uint32_t('X') << 8u) | (uint32_t('T') << 16u) | (uint32_t('1') << 24u);
const uint32_t FourCC_BC2_UNORM =
    (uint32_t('D') << 0u) | (uint32_t('X') << 8u) | (uint32_t('T') << 16u) | (uint32_t('3') << 24u);
const uint32_t FourCC_BC3_UNORM =
    (uint32_t('D') << 0u) | (uint32_t('X') << 8u) | (uint32_t('T') << 16u) | (uint32_t('5') << 24u);
const uint32_t FourCC_BC4_UNORM =
    (uint32_t('B') << 0u) | (uint32_t('C') << 8u) | (uint32_t('4') << 16u) | (uint32_t('U') << 24u);
const uint32_t FourCC_BC5_UNORM =
    (uint32_t('A') << 0u) | (uint32_t('T') << 8u) | (uint32_t('I') << 16u) | (uint32_t('2') << 24u);

struct DDSHeader {
    uint32_t dwMagic;
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];

    /*  DDPIXELFORMAT	*/
    struct {
        uint32_t dwSize;
        uint32_t dwFlags;
        uint32_t dwFourCC;
        uint32_t dwRGBBitCount;
        uint32_t dwRBitMask;
        uint32_t dwGBitMask;
        uint32_t dwBBitMask;
        uint32_t dwAlphaBitMask;
    } sPixelFormat;

    /*  DDCAPS2	*/
    struct {
        uint32_t dwCaps1;
        uint32_t dwCaps2;
        uint32_t dwDDSX;
        uint32_t dwReserved;
    } sCaps;
    uint32_t dwReserved2;
};
static_assert(sizeof(DDSHeader) == 128, "!");

enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
    DXGI_FORMAT_R32G32B32A32_UINT = 3,
    DXGI_FORMAT_R32G32B32A32_SINT = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS = 5,
    DXGI_FORMAT_R32G32B32_FLOAT = 6,
    DXGI_FORMAT_R32G32B32_UINT = 7,
    DXGI_FORMAT_R32G32B32_SINT = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM = 11,
    DXGI_FORMAT_R16G16B16A16_UINT = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM = 13,
    DXGI_FORMAT_R16G16B16A16_SINT = 14,
    DXGI_FORMAT_R32G32_TYPELESS = 15,
    DXGI_FORMAT_R32G32_FLOAT = 16,
    DXGI_FORMAT_R32G32_UINT = 17,
    DXGI_FORMAT_R32G32_SINT = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM = 24,
    DXGI_FORMAT_R10G10B10A2_UINT = 25,
    DXGI_FORMAT_R11G11B10_FLOAT = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
    DXGI_FORMAT_R8G8B8A8_UINT = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM = 31,
    DXGI_FORMAT_R8G8B8A8_SINT = 32,
    DXGI_FORMAT_R16G16_TYPELESS = 33,
    DXGI_FORMAT_R16G16_FLOAT = 34,
    DXGI_FORMAT_R16G16_UNORM = 35,
    DXGI_FORMAT_R16G16_UINT = 36,
    DXGI_FORMAT_R16G16_SNORM = 37,
    DXGI_FORMAT_R16G16_SINT = 38,
    DXGI_FORMAT_R32_TYPELESS = 39,
    DXGI_FORMAT_D32_FLOAT = 40,
    DXGI_FORMAT_R32_FLOAT = 41,
    DXGI_FORMAT_R32_UINT = 42,
    DXGI_FORMAT_R32_SINT = 43,
    DXGI_FORMAT_R24G8_TYPELESS = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
    DXGI_FORMAT_R8G8_TYPELESS = 48,
    DXGI_FORMAT_R8G8_UNORM = 49,
    DXGI_FORMAT_R8G8_UINT = 50,
    DXGI_FORMAT_R8G8_SNORM = 51,
    DXGI_FORMAT_R8G8_SINT = 52,
    DXGI_FORMAT_R16_TYPELESS = 53,
    DXGI_FORMAT_R16_FLOAT = 54,
    DXGI_FORMAT_D16_UNORM = 55,
    DXGI_FORMAT_R16_UNORM = 56,
    DXGI_FORMAT_R16_UINT = 57,
    DXGI_FORMAT_R16_SNORM = 58,
    DXGI_FORMAT_R16_SINT = 59,
    DXGI_FORMAT_R8_TYPELESS = 60,
    DXGI_FORMAT_R8_UNORM = 61,
    DXGI_FORMAT_R8_UINT = 62,
    DXGI_FORMAT_R8_SNORM = 63,
    DXGI_FORMAT_R8_SINT = 64,
    DXGI_FORMAT_A8_UNORM = 65,
    DXGI_FORMAT_R1_UNORM = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
    DXGI_FORMAT_BC1_TYPELESS = 70,
    DXGI_FORMAT_BC1_UNORM = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB = 72,
    DXGI_FORMAT_BC2_TYPELESS = 73,
    DXGI_FORMAT_BC2_UNORM = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB = 75,
    DXGI_FORMAT_BC3_TYPELESS = 76,
    DXGI_FORMAT_BC3_UNORM = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB = 78,
    DXGI_FORMAT_BC4_TYPELESS = 79,
    DXGI_FORMAT_BC4_UNORM = 80,
    DXGI_FORMAT_BC4_SNORM = 81,
    DXGI_FORMAT_BC5_TYPELESS = 82,
    DXGI_FORMAT_BC5_UNORM = 83,
    DXGI_FORMAT_BC5_SNORM = 84,
    DXGI_FORMAT_B5G6R5_UNORM = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
    DXGI_FORMAT_BC6H_TYPELESS = 94,
    DXGI_FORMAT_BC6H_UF16 = 95,
    DXGI_FORMAT_BC6H_SF16 = 96,
    DXGI_FORMAT_BC7_TYPELESS = 97,
    DXGI_FORMAT_BC7_UNORM = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB = 99,
    DXGI_FORMAT_AYUV = 100,
    DXGI_FORMAT_Y410 = 101,
    DXGI_FORMAT_Y416 = 102,
    DXGI_FORMAT_NV12 = 103,
    DXGI_FORMAT_P010 = 104,
    DXGI_FORMAT_P016 = 105,
    DXGI_FORMAT_420_OPAQUE = 106,
    DXGI_FORMAT_YUY2 = 107,
    DXGI_FORMAT_Y210 = 108,
    DXGI_FORMAT_Y216 = 109,
    DXGI_FORMAT_NV11 = 110,
    DXGI_FORMAT_AI44 = 111,
    DXGI_FORMAT_IA44 = 112,
    DXGI_FORMAT_P8 = 113,
    DXGI_FORMAT_A8P8 = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM = 115,
    DXGI_FORMAT_P208 = 130,
    DXGI_FORMAT_V208 = 131,
    DXGI_FORMAT_V408 = 132,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
    DXGI_FORMAT_FORCE_UINT = 0xffffffff
};

eTexFormat TexFormatFromDXGIFormat(DXGI_FORMAT f);

enum D3D10_RESOURCE_DIMENSION {
    D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
    D3D10_RESOURCE_DIMENSION_BUFFER = 1,
    D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
    D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
    D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
};

struct DDS_HEADER_DXT10 {
    DXGI_FORMAT dxgiFormat;
    D3D10_RESOURCE_DIMENSION resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};

extern const uint8_t _blank_DXT5_block_4x4[];
extern const int _blank_DXT5_block_4x4_len;

extern const uint8_t _blank_ASTC_block_4x4[];
extern const int _blank_ASTC_block_4x4_len;

float PerlinNoise(const Ren::Vec4f &P);
float PerlinNoise(const Ren::Vec4f &P, const Ren::Vec4f &rep);

//
// YUV image processing
//
void CopyYChannel_16px(const uint8_t *y_src, int y_stride, int w, int h, uint8_t *y_dst);
void CopyYChannel_32px(const uint8_t *y_src, int y_stride, int w, int h, uint8_t *y_dst);

void InterleaveUVChannels_16px(const uint8_t *u_src, const uint8_t *v_src, int u_stride, int v_stride, int w, int h,
                               uint8_t *uv_dst);

//
// BCn compression
//

// clang-format off

const int BlockSize_BC1 = 2 * sizeof(uint16_t) + sizeof(uint32_t);
//                        \_ low/high colors_/   \_ 16 x 2-bit _/
const int BlockSize_BC4 = 2 * sizeof(uint8_t) + 6 * sizeof(uint8_t);
//                        \_ low/high alpha_/     \_ 16 x 3-bit _/
const int BlockSize_BC3 = BlockSize_BC1 + BlockSize_BC4;
const int BlockSize_BC5 = BlockSize_BC4 + BlockSize_BC4;

// clang-format on

int GetRequiredMemory_BC1(int w, int h, int pitch_align);
int GetRequiredMemory_BC3(int w, int h, int pitch_align);
int GetRequiredMemory_BC4(int w, int h, int pitch_align);
int GetRequiredMemory_BC5(int w, int h, int pitch_align);

// NOTE: intended for realtime compression, quality may be not the best
template <int SrcChannels>
void CompressImage_BC1(const uint8_t img_src[], int w, int h, uint8_t img_dst[], int dst_pitch = 0);
template <bool Is_YCoCg = false>
void CompressImage_BC3(const uint8_t img_src[], int w, int h, uint8_t img_dst[], int dst_pitch = 0);
template <int SrcChannels = 1>
void CompressImage_BC4(const uint8_t img_src[], int w, int h, uint8_t img_dst[], int dst_pitch = 0);
template <int SrcChannels = 2>
void CompressImage_BC5(const uint8_t img_src[], int w, int h, uint8_t img_dst[], int dst_pitch = 0);
} // namespace Ren
