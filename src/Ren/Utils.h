#pragma once

#include <memory>

namespace Ren {
enum eTexColorFormat;
std::unique_ptr<uint8_t[]> ReadTGAFile(const void *data, int &w, int &h, eTexColorFormat &format);

std::unique_ptr<float[]> ConvertRGBE_to_RGB32F(const uint8_t *image_data, int w, int h);
std::unique_ptr<uint16_t[]> ConvertRGBE_to_RGB16F(const uint8_t *image_data, int w, int h);

std::unique_ptr<uint8_t[]> ConvertRGB32F_to_RGBE(const float *image_data, int w, int h, int channels);
std::unique_ptr<uint8_t[]> ConvertRGB32F_to_RGBM(const float *image_data, int w, int h, int channels);

int InitMipMaps(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16], int heights[16], int channels);

void ReorderTriangleIndices(const uint32_t *indices, uint32_t indices_count, uint32_t vtx_count, uint32_t *out_indices);

struct vertex_t {
    float p[3], n[3], b[3], t[2][2];
    int index;
};

void ComputeTextureBasis(std::vector<vertex_t> &vertices, std::vector<uint32_t> &new_vtx_indices,
                         const uint32_t *indices, size_t indices_count);

struct KTXHeader {
    char identifier[12] = { '\xAB', 'K', 'T', 'X', ' ', '1', '1', '\xBB', '\r', '\n', '\x1A', '\n' };
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

struct DDSHeader {
    uint32_t    dwMagic;
    uint32_t    dwSize;
    uint32_t    dwFlags;
    uint32_t    dwHeight;
    uint32_t    dwWidth;
    uint32_t    dwPitchOrLinearSize;
    uint32_t    dwDepth;
    uint32_t    dwMipMapCount;
    uint32_t    dwReserved1[11];

    /*  DDPIXELFORMAT	*/
    struct {
        uint32_t    dwSize;
        uint32_t    dwFlags;
        uint32_t    dwFourCC;
        uint32_t    dwRGBBitCount;
        uint32_t    dwRBitMask;
        uint32_t    dwGBitMask;
        uint32_t    dwBBitMask;
        uint32_t    dwAlphaBitMask;
    } sPixelFormat;

    /*  DDCAPS2	*/
    struct {
        uint32_t    dwCaps1;
        uint32_t    dwCaps2;
        uint32_t    dwDDSX;
        uint32_t    dwReserved;
    } sCaps;
    uint32_t    dwReserved2;
};
static_assert(sizeof(DDSHeader) == 128, "!");

extern const uint8_t _blank_DXT5_block_16x16[];
extern const int _blank_DXT5_block_16x16_len;

extern const uint8_t _blank_ASTC_block_16x16_8bb[];
extern const int _blank_ASTC_block_16x16_8bb_len;

int CalcMipCount(int w, int h, int min_res, eTexFilter filter);
}