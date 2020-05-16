#pragma once

#include <memory>

namespace Ren {
enum class eTexFormat;
std::unique_ptr<uint8_t[]> ReadTGAFile(const void *data, int &w, int &h, eTexFormat &format);

void RGBMDecode(const uint8_t rgbm[4], float out_rgb[3]);
void RGBMEncode(const float rgb[3], uint8_t out_rgbm[4]);

std::unique_ptr<float[]> ConvertRGBE_to_RGB32F(const uint8_t *image_data, int w, int h);
std::unique_ptr<uint16_t[]> ConvertRGBE_to_RGB16F(const uint8_t *image_data, int w, int h);

std::unique_ptr<uint8_t[]> ConvertRGB32F_to_RGBE(const float *image_data, int w, int h, int channels);
std::unique_ptr<uint8_t[]> ConvertRGB32F_to_RGBM(const float *image_data, int w, int h, int channels);

int InitMipMaps(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16], int heights[16], int channels);
int InitMipMapsRGBM(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16], int heights[16]);

void ReorderTriangleIndices(const uint32_t *indices, uint32_t indices_count, uint32_t vtx_count, uint32_t *out_indices);

struct vertex_t {
    float p[3], n[3], b[3], t[2][2];
    int index;
};

uint16_t f32_to_f16(float value);

void ComputeTextureBasis(std::vector<vertex_t> &vertices, std::vector<uint32_t> &new_vtx_indices,
                         const uint32_t *indices, size_t indices_count);

struct KTXHeader {  // NOLINT
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

extern const uint8_t _blank_DXT5_block_4x4[];
extern const int _blank_DXT5_block_4x4_len;

extern const uint8_t _blank_ASTC_block_4x4[];
extern const int _blank_ASTC_block_4x4_len;

int CalcMipCount(int w, int h, int min_res, eTexFilter filter);

float PerlinNoise(const Ren::Vec4f &P);
float PerlinNoise(const Ren::Vec4f &P, const Ren::Vec4f &rep);
}