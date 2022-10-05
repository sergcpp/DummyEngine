#include "Utils.h"

#include <deque>

#include "CPUFeatures.h"
#include "Texture.h"

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline
#endif

#define _MIN(x, y) ((x) < (y) ? (x) : (y))
#define _MAX(x, y) ((x) < (y) ? (y) : (x))
#define _ABS(x) ((x) < 0 ? -(x) : (x))
#define _CLAMP(x, lo, hi) (_MIN(_MAX((x), (lo)), (hi)))

#define _MIN3(x, y, z) _MIN((x), _MIN((y), (z)))
#define _MAX3(x, y, z) _MAX((x), _MAX((y), (z)))

#define _MIN4(x, y, z, w) _MIN(_MIN((x), (y)), _MIN((z), (w)))
#define _MAX4(x, y, z, w) _MAX(_MAX((x), (y)), _MAX((z), (w)))

namespace Ren {
const eTexFormat g_tex_format_from_dxgi_format[] = {
    eTexFormat::Undefined,          // DXGI_FORMAT_UNKNOWN
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32B32A32_TYPELESS
    eTexFormat::RawRGBA32F,         // DXGI_FORMAT_R32G32B32A32_FLOAT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32B32A32_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32B32A32_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32B32_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32B32_FLOAT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32B32_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32B32_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16B16A16_TYPELESS
    eTexFormat::RawRGBA16F,         // DXGI_FORMAT_R16G16B16A16_FLOAT
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16B16A16_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16B16A16_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16B16A16_SNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16B16A16_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32_TYPELESS
    eTexFormat::RawRG32F,           // DXGI_FORMAT_R32G32_FLOAT
    eTexFormat::RawRG32UI,          // DXGI_FORMAT_R32G32_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G32_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32G8X24_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_D32_FLOAT_S8X24_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_X32_TYPELESS_G8X24_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R10G10B10A2_TYPELESS
    eTexFormat::RawRGB10_A2,        // DXGI_FORMAT_R10G10B10A2_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R10G10B10A2_UINT
    eTexFormat::RawRG11F_B10F,      // DXGI_FORMAT_R11G11B10_FLOAT
    eTexFormat::Undefined,          // DXGI_FORMAT_R8G8B8A8_TYPELESS
    eTexFormat::RawRGBA8888,        // DXGI_FORMAT_R8G8B8A8_UNORM
    eTexFormat::RawRGBA8888,        // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    eTexFormat::Undefined,          // DXGI_FORMAT_R8G8B8A8_UINT
    eTexFormat::RawRGBA8888Snorm,   // DXGI_FORMAT_R8G8B8A8_SNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R8G8B8A8_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16_TYPELESS
    eTexFormat::RawRG16F,           // DXGI_FORMAT_R16G16_FLOAT
    eTexFormat::RawRG16,            // DXGI_FORMAT_R16G16_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16_SNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R16G16_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_D32_FLOAT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32_FLOAT
    eTexFormat::RawR32UI,           // DXGI_FORMAT_R32_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R32_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R24G8_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_D24_UNORM_S8_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_X24_TYPELESS_G8_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R8G8_TYPELESS
    eTexFormat::RawRG88,            // DXGI_FORMAT_R8G8_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R8G8_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R8G8_SNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R8G8_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R16_TYPELESS
    eTexFormat::RawR16F,            // DXGI_FORMAT_R16_FLOAT
    eTexFormat::Undefined,          // DXGI_FORMAT_D16_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R16_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R16_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R16_SNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R16_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R8_TYPELESS
    eTexFormat::RawR8,              // DXGI_FORMAT_R8_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R8_UINT
    eTexFormat::Undefined,          // DXGI_FORMAT_R8_SNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R8_SINT
    eTexFormat::Undefined,          // DXGI_FORMAT_A8_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R1_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R9G9B9E5_SHAREDEXP
    eTexFormat::Undefined,          // DXGI_FORMAT_R8G8_B8G8_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_G8R8_G8B8_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_BC1_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_BC1_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_BC1_UNORM_SRGB
    eTexFormat::Undefined,          // DXGI_FORMAT_BC2_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_BC2_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_BC2_UNORM_SRGB
    eTexFormat::Undefined,          // DXGI_FORMAT_BC3_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_BC3_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_BC3_UNORM_SRGB
    eTexFormat::Undefined,          // DXGI_FORMAT_BC4_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_BC4_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_BC4_SNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_BC5_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_BC5_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_BC5_SNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_B5G6R5_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_B5G5R5A1_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_B8G8R8A8_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_B8G8R8X8_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_B8G8R8A8_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
    eTexFormat::Undefined,          // DXGI_FORMAT_B8G8R8X8_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
    eTexFormat::Undefined,          // DXGI_FORMAT_BC6H_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_BC6H_UF16
    eTexFormat::Undefined,          // DXGI_FORMAT_BC6H_SF16
    eTexFormat::Undefined,          // DXGI_FORMAT_BC7_TYPELESS
    eTexFormat::Undefined,          // DXGI_FORMAT_BC7_UNORM
    eTexFormat::Undefined,          // DXGI_FORMAT_BC7_UNORM_SRGB
    eTexFormat::Undefined,          // DXGI_FORMAT_AYUV
    eTexFormat::Undefined,          // DXGI_FORMAT_Y410
    eTexFormat::Undefined,          // DXGI_FORMAT_Y416
    eTexFormat::Undefined,          // DXGI_FORMAT_NV12
    eTexFormat::Undefined,          //  DXGI_FORMAT_P010
    eTexFormat::Undefined,          // DXGI_FORMAT_P016
    eTexFormat::Undefined,          // DXGI_FORMAT_420_OPAQUE
    eTexFormat::Undefined,          // DXGI_FORMAT_YUY2
    eTexFormat::Undefined,          // DXGI_FORMAT_Y210
    eTexFormat::Undefined,          // DXGI_FORMAT_Y216
    eTexFormat::Undefined,          // DXGI_FORMAT_NV11
    eTexFormat::Undefined,          // DXGI_FORMAT_AI44
    eTexFormat::Undefined,          // DXGI_FORMAT_IA44
    eTexFormat::Undefined,          // DXGI_FORMAT_P8
    eTexFormat::Undefined,          // DXGI_FORMAT_A8P8
    eTexFormat::Undefined,          // DXGI_FORMAT_B4G4R4A4_UNORM = 115
};
static_assert(sizeof(g_tex_format_from_dxgi_format) / sizeof(g_tex_format_from_dxgi_format[0]) == 116, "!");

uint16_t f32_to_f16(const float value) {
    int32_t i;
    memcpy(&i, &value, sizeof(float));

    int32_t s = (i >> 16) & 0x00008000;
    int32_t e = ((i >> 23) & 0x000000ff) - (127 - 15);
    int32_t m = i & 0x007fffff;
    if (e <= 0) {
        if (e < -10) {
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        }

        m = (m | 0x00800000) >> (1 - e);

        if (m & 0x00001000)
            m += 0x00002000;

        s = s | (m >> 13);
        uint16_t ret;
        memcpy(&ret, &s, sizeof(uint16_t));
        return ret;
    } else if (e == 0xff - (127 - 15)) {
        if (m == 0) {
            s = s | 0x7c00;
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        } else {
            m >>= 13;

            s = s | 0x7c00 | m | (m == 0);
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        }
    } else {
        if (m & 0x00001000) {
            m += 0x00002000;

            if (m & 0x00800000) {
                m = 0;  // overflow in significand,
                e += 1; // adjust exponent
            }
        }

        if (e > 30) {
            s = s | 0x7c00;
            uint16_t ret;
            memcpy(&ret, &s, sizeof(uint16_t));
            return ret;
        }

        s = s | (e << 10) | (m >> 13);
        uint16_t ret;
        memcpy(&ret, &s, sizeof(uint16_t));
        return ret;
    }
}

force_inline int16_t f32_to_s16(const float value) { return int16_t(value * 32767); }

force_inline uint16_t f32_to_u16(const float value) { return uint16_t(value * 65535); }

/*
    RGB <-> YCoCg

    Y  = [ 1/4  1/2   1/4] [R]
    Co = [ 1/2    0  -1/2] [G]
    CG = [-1/4  1/2  -1/4] [B]

    R  = [   1    1    -1] [Y]
    G  = [   1    0     1] [Co]
    B  = [   1   -1    -1] [Cg]
*/

force_inline uint8_t to_clamped_uint8(const int x) { return ((x) < 0 ? (0) : ((x) > 255 ? 255 : (x))); }

//
// Perfectly reversible RGB <-> YCoCg conversion (relies on integer wrap around)
//

force_inline void RGB_to_YCoCg_reversible(const uint8_t in_RGB[3], uint8_t out_YCoCg[3]) {
    out_YCoCg[1] = in_RGB[0] - in_RGB[2];
    const uint8_t t = in_RGB[2] + (out_YCoCg[1] >> 1);
    out_YCoCg[2] = in_RGB[1] - t;
    out_YCoCg[0] = t + (out_YCoCg[2] >> 1);
}

force_inline void YCoCg_to_RGB_reversible(const uint8_t in_YCoCg[3], uint8_t out_RGB[3]) {
    const uint8_t t = in_YCoCg[0] - (in_YCoCg[2] >> 1);
    out_RGB[1] = in_YCoCg[2] + t;
    out_RGB[2] = t - (in_YCoCg[1] >> 1);
    out_RGB[0] = in_YCoCg[1] + out_RGB[2];
}

//
// Not-so-perfectly reversible RGB <-> YCoCg conversion (to use in shaders)
//

force_inline void RGB_to_YCoCg(const uint8_t in_RGB[3], uint8_t out_YCoCg[3]) {
    const int R = int(in_RGB[0]);
    const int G = int(in_RGB[1]);
    const int B = int(in_RGB[2]);

    out_YCoCg[0] = (R + 2 * G + B) / 4;
    out_YCoCg[1] = to_clamped_uint8(128 + (R - B) / 2);
    out_YCoCg[2] = to_clamped_uint8(128 + (-R + 2 * G - B) / 4);
}

force_inline void YCoCg_to_RGB(const uint8_t in_YCoCg[3], uint8_t out_RGB[3]) {
    const int Y = int(in_YCoCg[0]);
    const int Co = int(in_YCoCg[1]) - 128;
    const int Cg = int(in_YCoCg[2]) - 128;

    out_RGB[0] = to_clamped_uint8(Y + Co - Cg);
    out_RGB[1] = to_clamped_uint8(Y + Cg);
    out_RGB[2] = to_clamped_uint8(Y - Co - Cg);
}

const uint8_t _blank_DXT5_block_4x4[] = {0x00, 0x00, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const int _blank_DXT5_block_4x4_len = sizeof(_blank_DXT5_block_4x4);

const uint8_t _blank_ASTC_block_4x4[] = {0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const int _blank_ASTC_block_4x4_len = sizeof(_blank_ASTC_block_4x4);

force_inline Ren::Vec4f permute(const Ren::Vec4f &x) {
    return Ren::Mod(((x * 34.0) + Ren::Vec4f{1.0}) * x, Ren::Vec4f{289.0});
}

force_inline Ren::Vec4f taylor_inv_sqrt(const Ren::Vec4f &r) {
    return Ren::Vec4f{1.79284291400159f} - 0.85373472095314f * r;
}

force_inline Ren::Vec4f fade(const Ren::Vec4f &t) {
    return t * t * t * (t * (t * 6.0f - Ren::Vec4f{15.0f}) + Ren::Vec4f{10.0f});
}

} // namespace Ren

std::unique_ptr<uint8_t[]> Ren::ReadTGAFile(const void *data, int &w, int &h, eTexFormat &format) {
    uint32_t img_size;
    ReadTGAFile(data, w, h, format, nullptr, img_size);

    std::unique_ptr<uint8_t[]> image_ret(new uint8_t[img_size]);
    ReadTGAFile(data, w, h, format, image_ret.get(), img_size);

    return image_ret;
}

bool Ren::ReadTGAFile(const void *data, int &w, int &h, eTexFormat &format, uint8_t *out_data, uint32_t &out_size) {
    const uint8_t tga_header[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto *tga_compare = (const uint8_t *)data;
    const uint8_t *img_header = (const uint8_t *)data + sizeof(tga_header);
    bool compressed = false;

    if (memcmp(tga_header, tga_compare, sizeof(tga_header)) != 0) {
        if (tga_compare[2] == 1) {
            fprintf(stderr, "Image cannot be indexed color.");
            return false;
        }
        if (tga_compare[2] == 3) {
            fprintf(stderr, "Image cannot be greyscale color.");
            return false;
        }
        if (tga_compare[2] == 9 || tga_compare[2] == 10) {
            compressed = true;
        }
    }

    w = int(img_header[1] * 256u + img_header[0]);
    h = int(img_header[3] * 256u + img_header[2]);

    if (w <= 0 || h <= 0 || (img_header[4] != 24 && img_header[4] != 32)) {
        if (w <= 0 || h <= 0) {
            fprintf(stderr, "Image must have a width and height greater than 0");
        }
        if (img_header[4] != 24 && img_header[4] != 32) {
            fprintf(stderr, "Image must be 24 or 32 bit");
        }
        return false;
    }

    const uint32_t bpp = img_header[4];
    const uint32_t bytes_per_pixel = bpp / 8;
    if (bpp == 32) {
        format = eTexFormat::RawRGBA8888;
    } else if (bpp == 24) {
        format = eTexFormat::RawRGB888;
    }

    if (out_data && out_size < w * h * bytes_per_pixel) {
        return false;
    }

    out_size = w * h * bytes_per_pixel;
    if (out_data) {
        const uint8_t *image_data = (const uint8_t *)data + 18;

        if (!compressed) {
            for (size_t i = 0; i < out_size; i += bytes_per_pixel) {
                out_data[i] = image_data[i + 2];
                out_data[i + 1] = image_data[i + 1];
                out_data[i + 2] = image_data[i];
                if (bytes_per_pixel == 4) {
                    out_data[i + 3] = image_data[i + 3];
                }
            }
        } else {
            for (size_t num = 0; num < out_size;) {
                uint8_t packet_header = *image_data++;
                if (packet_header & (1u << 7u)) {
                    uint8_t color[4];
                    unsigned size = (packet_header & ~(1u << 7u)) + 1;
                    size *= bytes_per_pixel;
                    for (unsigned i = 0; i < bytes_per_pixel; i++) {
                        color[i] = *image_data++;
                    }
                    for (unsigned i = 0; i < size; i += bytes_per_pixel, num += bytes_per_pixel) {
                        out_data[num] = color[2];
                        out_data[num + 1] = color[1];
                        out_data[num + 2] = color[0];
                        if (bytes_per_pixel == 4) {
                            out_data[num + 3] = color[3];
                        }
                    }
                } else {
                    unsigned size = (packet_header & ~(1u << 7u)) + 1;
                    size *= bytes_per_pixel;
                    for (unsigned i = 0; i < size; i += bytes_per_pixel, num += bytes_per_pixel) {
                        out_data[num] = image_data[i + 2];
                        out_data[num + 1] = image_data[i + 1];
                        out_data[num + 2] = image_data[i];
                        if (bytes_per_pixel == 4) {
                            out_data[num + 3] = image_data[i + 3];
                        }
                    }
                    image_data += size;
                }
            }
        }
    }

    return true;
}

void Ren::RGBMDecode(const uint8_t rgbm[4], float out_rgb[3]) {
    out_rgb[0] = 4.0f * (rgbm[0] / 255.0f) * (rgbm[3] / 255.0f);
    out_rgb[1] = 4.0f * (rgbm[1] / 255.0f) * (rgbm[3] / 255.0f);
    out_rgb[2] = 4.0f * (rgbm[2] / 255.0f) * (rgbm[3] / 255.0f);
}

void Ren::RGBMEncode(const float rgb[3], uint8_t out_rgbm[4]) {
    float fr = rgb[0] / 4.0f;
    float fg = rgb[1] / 4.0f;
    float fb = rgb[2] / 4.0f;
    float fa = std::max(std::max(fr, fg), std::max(fb, 1e-6f));
    if (fa > 1.0f)
        fa = 1.0f;

    fa = std::ceil(fa * 255.0f) / 255.0f;
    fr /= fa;
    fg /= fa;
    fb /= fa;

    out_rgbm[0] = (uint8_t)_CLAMP(int(fr * 255), 0, 255);
    out_rgbm[1] = (uint8_t)_CLAMP(int(fg * 255), 0, 255);
    out_rgbm[2] = (uint8_t)_CLAMP(int(fb * 255), 0, 255);
    out_rgbm[3] = (uint8_t)_CLAMP(int(fa * 255), 0, 255);
}

std::unique_ptr<float[]> Ren::ConvertRGBE_to_RGB32F(const uint8_t image_data[], const int w, const int h) {
    std::unique_ptr<float[]> fp_data(new float[w * h * 3]);

    for (int i = 0; i < w * h; i++) {
        const uint8_t r = image_data[4 * i + 0], g = image_data[4 * i + 1], b = image_data[4 * i + 2],
                      a = image_data[4 * i + 3];

        const float f = std::exp2(float(a) - 128.0f);
        const float k = 1.0f / 255;

        fp_data[3 * i + 0] = k * float(r) * f;
        fp_data[3 * i + 1] = k * float(g) * f;
        fp_data[3 * i + 2] = k * float(b) * f;
    }

    return fp_data;
}

std::unique_ptr<uint16_t[]> Ren::ConvertRGBE_to_RGB16F(const uint8_t image_data[], const int w, const int h) {
    std::unique_ptr<uint16_t[]> fp16_data(new uint16_t[w * h * 3]);
    ConvertRGBE_to_RGB16F(image_data, w, h, fp16_data.get());
    return fp16_data;
}

void Ren::ConvertRGBE_to_RGB16F(const uint8_t image_data[], int w, int h, uint16_t *out_data) {
    for (int i = 0; i < w * h; i++) {
        const uint8_t r = image_data[4 * i + 0], g = image_data[4 * i + 1], b = image_data[4 * i + 2],
                      a = image_data[4 * i + 3];

        const float f = std::exp2(float(a) - 128.0f);
        const float k = 1.0f / 255;

        out_data[3 * i + 0] = f32_to_f16(k * float(r) * f);
        out_data[3 * i + 1] = f32_to_f16(k * float(g) * f);
        out_data[3 * i + 2] = f32_to_f16(k * float(b) * f);
    }
}

std::unique_ptr<uint8_t[]> Ren::ConvertRGB32F_to_RGBE(const float image_data[], const int w, const int h,
                                                      const int channels) {
    std::unique_ptr<uint8_t[]> u8_data(new uint8_t[w * h * 4]);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            auto val = Vec3f{Uninitialize};

            if (channels == 3) {
                val[0] = image_data[3 * (y * w + x) + 0];
                val[1] = image_data[3 * (y * w + x) + 1];
                val[2] = image_data[3 * (y * w + x) + 2];
            } else if (channels == 4) {
                val[0] = image_data[4 * (y * w + x) + 0];
                val[1] = image_data[4 * (y * w + x) + 1];
                val[2] = image_data[4 * (y * w + x) + 2];
            }

            auto exp = Vec3f{std::log2(val[0]), std::log2(val[1]), std::log2(val[2])};
            for (int i = 0; i < 3; i++) {
                exp[i] = std::ceil(exp[i]);
                if (exp[i] < -128.0f) {
                    exp[i] = -128.0f;
                } else if (exp[i] > 127.0f) {
                    exp[i] = 127.0f;
                }
            }

            const float common_exp = std::max(exp[0], std::max(exp[1], exp[2]));
            const float range = std::exp2(common_exp);

            Ren::Vec3f mantissa = val / range;
            for (int i = 0; i < 3; i++) {
                if (mantissa[i] < 0.0f)
                    mantissa[i] = 0.0f;
                else if (mantissa[i] > 1.0f)
                    mantissa[i] = 1.0f;
            }

            const auto res = Ren::Vec4f{mantissa[0], mantissa[1], mantissa[2], common_exp + 128.0f};

            u8_data[(y * w + x) * 4 + 0] = (uint8_t)_CLAMP(int(res[0] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 1] = (uint8_t)_CLAMP(int(res[1] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 2] = (uint8_t)_CLAMP(int(res[2] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 3] = (uint8_t)_CLAMP(int(res[3]), 0, 255);
        }
    }

    return u8_data;
}

std::unique_ptr<uint8_t[]> Ren::ConvertRGB32F_to_RGBM(const float image_data[], const int w, const int h,
                                                      const int channels) {
    std::unique_ptr<uint8_t[]> u8_data(new uint8_t[w * h * 4]);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            RGBMEncode(&image_data[channels * (y * w + x)], &u8_data[(y * w + x) * 4]);
        }
    }

    return u8_data;
}

void Ren::ConvertRGB_to_YCoCg_rev(const uint8_t in_RGB[3], uint8_t out_YCoCg[3]) {
    RGB_to_YCoCg_reversible(in_RGB, out_YCoCg);
}

void Ren::ConvertYCoCg_to_RGB_rev(const uint8_t in_YCoCg[3], uint8_t out_RGB[3]) {
    YCoCg_to_RGB_reversible(in_YCoCg, out_RGB);
}

std::unique_ptr<uint8_t[]> Ren::ConvertRGB_to_CoCgxY_rev(const uint8_t image_data[], const int w, const int h) {
    std::unique_ptr<uint8_t[]> u8_data(new uint8_t[w * h * 4]);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t YCoCg[3];
            RGB_to_YCoCg_reversible(&image_data[(y * w + x) * 3], YCoCg);

            u8_data[(y * w + x) * 4 + 0] = YCoCg[1];
            u8_data[(y * w + x) * 4 + 1] = YCoCg[2];
            u8_data[(y * w + x) * 4 + 2] = 0;
            u8_data[(y * w + x) * 4 + 3] = YCoCg[0];
        }
    }

    return u8_data;
}

std::unique_ptr<uint8_t[]> Ren::ConvertCoCgxY_to_RGB_rev(const uint8_t image_data[], const int w, const int h) {
    std::unique_ptr<uint8_t[]> u8_data(new uint8_t[w * h * 3]);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const uint8_t YCoCg[] = {image_data[(y * w + x) * 4 + 3], image_data[(y * w + x) * 4 + 0],
                                     image_data[(y * w + x) * 4 + 1]};
            YCoCg_to_RGB_reversible(YCoCg, &u8_data[(y * w + x) * 3]);
        }
    }

    return u8_data;
}

void Ren::ConvertRGB_to_YCoCg(const uint8_t in_RGB[3], uint8_t out_YCoCg[3]) { RGB_to_YCoCg(in_RGB, out_YCoCg); }
void Ren::ConvertYCoCg_to_RGB(const uint8_t in_YCoCg[3], uint8_t out_RGB[3]) { YCoCg_to_RGB(in_YCoCg, out_RGB); }

std::unique_ptr<uint8_t[]> Ren::ConvertRGB_to_CoCgxY(const uint8_t image_data[], const int w, const int h) {
    std::unique_ptr<uint8_t[]> u8_data(new uint8_t[w * h * 4]);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t YCoCg[3];
            RGB_to_YCoCg(&image_data[(y * w + x) * 3], YCoCg);

            u8_data[(y * w + x) * 4 + 0] = YCoCg[1];
            u8_data[(y * w + x) * 4 + 1] = YCoCg[2];
            u8_data[(y * w + x) * 4 + 2] = 0;
            u8_data[(y * w + x) * 4 + 3] = YCoCg[0];
        }
    }

    return u8_data;
}

std::unique_ptr<uint8_t[]> Ren::ConvertCoCgxY_to_RGB(const uint8_t image_data[], const int w, const int h) {
    std::unique_ptr<uint8_t[]> u8_data(new uint8_t[w * h * 3]);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const uint8_t YCoCg[] = {image_data[(y * w + x) * 4 + 3], image_data[(y * w + x) * 4 + 0],
                                     image_data[(y * w + x) * 4 + 1]};
            YCoCg_to_RGB(YCoCg, &u8_data[(y * w + x) * 3]);
        }
    }

    return u8_data;
}

Ren::eTexFormat Ren::TexFormatFromDXGIFormat(const DXGI_FORMAT f) { return g_tex_format_from_dxgi_format[int(f)]; }

int Ren::InitMipMaps(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16], int heights[16], const int channels,
                     const eMipOp op[4]) {
    int mip_count = 1;

    int _w = widths[0], _h = heights[0];
    while (_w > 1 || _h > 1) {
        int _prev_w = _w, _prev_h = _h;
        _w = std::max(_w / 2, 1);
        _h = std::max(_h / 2, 1);
        if (!mipmaps[mip_count]) {
            mipmaps[mip_count].reset(new uint8_t[_w * _h * channels]);
        }
        widths[mip_count] = _w;
        heights[mip_count] = _h;
        const uint8_t *tex = mipmaps[mip_count - 1].get();

        int count = 0;

        for (int j = 0; j < _prev_h; j += 2) {
            for (int i = 0; i < _prev_w; i += 2) {
                for (int k = 0; k < channels; k++) {
                    if (op[k] == eMipOp::Skip) {
                        continue;
                    } else if (op[k] == eMipOp::Zero) {
                        mipmaps[mip_count][count * channels + k] = 0;
                    }

                    // 4x4 pixel neighbourhood
                    int c[4][4];

                    // fetch inner quad
                    // TODO: optimize this!
                    c[1][1] = tex[((j + 0) * _prev_w + i + 0) * channels + k];
                    if (i + 1 < _prev_w) {
                        c[1][2] = tex[((j + 0) * _prev_w + i + 1) * channels + k];
                    } else {
                        c[1][2] = c[1][1];
                    }
                    if (j + 1 < _prev_h) {
                        c[2][1] = tex[((j + 1) * _prev_w + i + 0) * channels + k];
                        c[2][2] = tex[((j + 1) * _prev_w + i + 1) * channels + k];
                    } else {
                        c[2][1] = c[2][2] = c[1][1];
                    }

                    if (op[k] == eMipOp::Avg) {
                        mipmaps[mip_count][count * channels + k] = uint8_t((c[1][1] + c[1][2] + c[2][1] + c[2][2]) / 4);
                    } else if (op[k] == eMipOp::Min) {
                        mipmaps[mip_count][count * channels + k] = uint8_t(_MIN4(c[1][1], c[1][2], c[2][1], c[2][2]));
                    } else if (op[k] == eMipOp::Max) {
                        mipmaps[mip_count][count * channels + k] = uint8_t(_MAX4(c[1][1], c[1][2], c[2][1], c[2][2]));
                    } else if (op[k] == eMipOp::MinBilinear || op[k] == eMipOp::MaxBilinear) {

                        // fetch outer quad
                        for (int dy = -1; dy < 3; dy++) {
                            for (int dx = -1; dx < 3; dx++) {
                                if ((dx == 0 || dx == 1) && (dy == 0 || dy == 1)) {
                                    continue;
                                }

                                const int i0 = (i + dx + _prev_w) % _prev_w;
                                const int j0 = (j + dy + _prev_h) % _prev_h;

                                c[dy + 1][dx + 1] = tex[(j0 * _prev_w + i0) * channels + k];
                            }
                        }

                        static const int quadrants[2][2][2] = {{{-1, -1}, {+1, -1}}, {{-1, +1}, {+1, +1}}};

                        int test_val = c[1][1];

                        for (int dj = 1; dj < 3; dj++) {
                            for (int di = 1; di < 3; di++) {
                                const int i0 = di + quadrants[dj - 1][di - 1][0];
                                const int j0 = dj + quadrants[dj - 1][di - 1][1];

                                if (op[k] == eMipOp::MinBilinear) {
                                    test_val = _MIN(test_val, (c[dj][di] + c[dj][i0]) / 2);
                                    test_val = _MIN(test_val, (c[dj][di] + c[j0][di]) / 2);
                                } else if (op[k] == eMipOp::MaxBilinear) {
                                    test_val = _MAX(test_val, (c[dj][di] + c[dj][i0]) / 2);
                                    test_val = _MAX(test_val, (c[dj][di] + c[j0][di]) / 2);
                                }
                            }
                        }

                        for (int dj = 0; dj < 3; dj++) {
                            for (int di = 0; di < 3; di++) {
                                if (di == 1 && dj == 1) {
                                    continue;
                                }

                                if (op[k] == eMipOp::MinBilinear) {
                                    test_val = _MIN(test_val, (c[dj + 0][di + 0] + c[dj + 0][di + 1] +
                                                               c[dj + 1][di + 0] + c[dj + 1][di + 1]) /
                                                                  4);
                                } else if (op[k] == eMipOp::MaxBilinear) {
                                    test_val = _MAX(test_val, (c[dj + 0][di + 0] + c[dj + 0][di + 1] +
                                                               c[dj + 1][di + 0] + c[dj + 1][di + 1]) /
                                                                  4);
                                }
                            }
                        }

                        c[1][1] = test_val;

                        if (op[k] == eMipOp::MinBilinear) {
                            mipmaps[mip_count][count * channels + k] =
                                uint8_t(_MIN4(c[1][1], c[1][2], c[2][1], c[2][2]));
                        } else if (op[k] == eMipOp::MaxBilinear) {
                            mipmaps[mip_count][count * channels + k] =
                                uint8_t(_MAX4(c[1][1], c[1][2], c[2][1], c[2][2]));
                        }
                    }
                }

                count++;
            }
        }

        mip_count++;
    }

    return mip_count;
}

int Ren::InitMipMapsRGBM(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16], int heights[16]) {
    int mip_count = 1;

    int _w = widths[0], _h = heights[0];
    while (_w > 1 || _h > 1) {
        int _prev_w = _w, _prev_h = _h;
        _w = std::max(_w / 2, 1);
        _h = std::max(_h / 2, 1);
        mipmaps[mip_count].reset(new uint8_t[_w * _h * 4]);
        widths[mip_count] = _w;
        heights[mip_count] = _h;
        const uint8_t *tex = mipmaps[mip_count - 1].get();

        int count = 0;

        for (int j = 0; j < _prev_h; j += 2) {
            for (int i = 0; i < _prev_w; i += 2) {
                float rgb_sum[3];
                RGBMDecode(&tex[((j + 0) * _prev_w + i) * 4], rgb_sum);

                float temp[3];
                RGBMDecode(&tex[((j + 0) * _prev_w + i + 1) * 4], temp);
                rgb_sum[0] += temp[0];
                rgb_sum[1] += temp[1];
                rgb_sum[2] += temp[2];

                RGBMDecode(&tex[((j + 1) * _prev_w + i) * 4], temp);
                rgb_sum[0] += temp[0];
                rgb_sum[1] += temp[1];
                rgb_sum[2] += temp[2];

                RGBMDecode(&tex[((j + 1) * _prev_w + i + 1) * 4], temp);
                rgb_sum[0] += temp[0];
                rgb_sum[1] += temp[1];
                rgb_sum[2] += temp[2];

                rgb_sum[0] /= 4.0f;
                rgb_sum[1] /= 4.0f;
                rgb_sum[2] /= 4.0f;

                RGBMEncode(rgb_sum, &mipmaps[mip_count][count * 4]);
                count++;
            }
        }

        mip_count++;
    }

    return mip_count;
}

void Ren::ReorderTriangleIndices(const uint32_t *indices, const uint32_t indices_count, const uint32_t vtx_count,
                                 uint32_t *out_indices) {
    // From https://tomforsyth1000.github.io/papers/fast_vert_cache_opt.html

    uint32_t prim_count = indices_count / 3;

    struct vtx_data_t {
        int32_t cache_pos = -1;
        float score = 0.0f;
        uint32_t ref_count = 0;
        uint32_t active_tris_count = 0;
        std::unique_ptr<int32_t[]> tris;
    };

    const int MaxSizeVertexCache = 32;

    auto get_vertex_score = [MaxSizeVertexCache](int32_t cache_pos, uint32_t active_tris_count) -> float {
        const float CacheDecayPower = 1.5f;
        const float LastTriScore = 0.75f;
        const float ValenceBoostScale = 2.0f;
        const float ValenceBoostPower = 0.5f;

        if (active_tris_count == 0) {
            // No tri needs this vertex!
            return -1.0f;
        }

        float score = 0.0f;

        if (cache_pos < 0) {
            // Vertex is not in FIFO cache - no score.
        } else if (cache_pos < 3) {
            // This vertex was used in the last triangle,
            // so it has a fixed score, whichever of the three
            // it's in. Otherwise, you can get very different
            // answers depending on whether you add
            // the triangle 1,2,3 or 3,1,2 - which is silly.
            score = LastTriScore;
        } else {
            assert(cache_pos < MaxSizeVertexCache);
            // Points for being high in the cache.
            const float scaler = 1.0f / (MaxSizeVertexCache - 3);
            score = 1.0f - float(cache_pos - 3) * scaler;
            score = std::pow(score, CacheDecayPower);
        }

        // Bonus points for having a low number of tris still to
        // use the vert, so we get rid of lone verts quickly.

        const float valence_boost = std::pow((float)active_tris_count, -ValenceBoostPower);
        score += ValenceBoostScale * valence_boost;
        return score;
    };

    struct tri_data_t {
        bool is_in_list = false;
        float score = 0.0f;
        uint32_t indices[3] = {};
    };

    std::unique_ptr<vtx_data_t[]> _vertices(new vtx_data_t[vtx_count]);
    std::unique_ptr<tri_data_t[]> _triangles(new tri_data_t[prim_count]);

    // avoid operator[] call overhead in debug
    vtx_data_t *vertices = _vertices.get();
    tri_data_t *triangles = _triangles.get();

    for (uint32_t i = 0; i < indices_count; i += 3) {
        tri_data_t &tri = triangles[i / 3];

        tri.indices[0] = indices[i + 0];
        tri.indices[1] = indices[i + 1];
        tri.indices[2] = indices[i + 2];

        vertices[indices[i + 0]].active_tris_count++;
        vertices[indices[i + 1]].active_tris_count++;
        vertices[indices[i + 2]].active_tris_count++;
    }

    for (uint32_t i = 0; i < vtx_count; i++) {
        vtx_data_t &v = vertices[i];
        v.tris.reset(new int32_t[v.active_tris_count]);
        v.score = get_vertex_score(v.cache_pos, v.active_tris_count);
    }

    int32_t next_best_index = -1, next_next_best_index = -1;
    float next_best_score = -1.0f, next_next_best_score = -1.0f;

    for (uint32_t i = 0; i < indices_count; i += 3) {
        tri_data_t &tri = triangles[i / 3];

        vtx_data_t &v0 = vertices[indices[i + 0]];
        vtx_data_t &v1 = vertices[indices[i + 1]];
        vtx_data_t &v2 = vertices[indices[i + 2]];

        v0.tris[v0.ref_count++] = i / 3;
        v1.tris[v1.ref_count++] = i / 3;
        v2.tris[v2.ref_count++] = i / 3;

        tri.score = v0.score + v1.score + v2.score;

        if (tri.score > next_best_score) {
            if (next_best_score > next_next_best_score) {
                next_next_best_index = next_best_index;
                next_next_best_score = next_best_score;
            }
            next_best_index = i / 3;
            next_best_score = tri.score;
        }

        if (tri.score > next_next_best_score) {
            next_next_best_index = i / 3;
            next_next_best_score = tri.score;
        }
    }

    std::deque<uint32_t> lru_cache;

    auto use_vertex = [](std::deque<uint32_t> &lru_cache, uint32_t vtx_index) {
        auto it = std::find(std::begin(lru_cache), std::end(lru_cache), vtx_index);

        if (it == std::end(lru_cache)) {
            lru_cache.push_back(vtx_index);
            it = std::begin(lru_cache);
        }

        if (it != std::begin(lru_cache)) {
            lru_cache.erase(it);
            lru_cache.push_front(vtx_index);
        }
    };

    auto enforce_size = [&get_vertex_score](std::deque<uint32_t> &lru_cache, vtx_data_t *vertices, uint32_t max_size,
                                            std::vector<uint32_t> &out_tris_to_update) {
        out_tris_to_update.clear();

        if (lru_cache.size() > max_size) {
            lru_cache.resize(max_size);
        }

        for (size_t i = 0; i < lru_cache.size(); i++) {
            vtx_data_t &v = vertices[lru_cache[i]];

            v.cache_pos = (int32_t)i;
            v.score = get_vertex_score(v.cache_pos, v.active_tris_count);

            for (uint32_t j = 0; j < v.ref_count; j++) {
                int tri_index = v.tris[j];
                if (tri_index != -1) {
                    auto it = std::find(std::begin(out_tris_to_update), std::end(out_tris_to_update), tri_index);
                    if (it == std::end(out_tris_to_update)) {
                        out_tris_to_update.push_back(tri_index);
                    }
                }
            }
        }
    };

    for (int32_t out_index = 0; out_index < (int32_t)indices_count;) {
        if (next_best_index < 0) {
            next_best_score = next_next_best_score = -1.0f;
            next_best_index = next_next_best_index = -1;

            for (int32_t i = 0; i < (int32_t)prim_count; i++) {
                const tri_data_t &tri = triangles[i];
                if (!tri.is_in_list) {
                    if (tri.score > next_best_score) {
                        if (next_best_score > next_next_best_score) {
                            next_next_best_index = next_best_index;
                            next_next_best_score = next_best_score;
                        }
                        next_best_index = i;
                        next_best_score = tri.score;
                    }

                    if (tri.score > next_next_best_score) {
                        next_next_best_index = i;
                        next_next_best_score = tri.score;
                    }
                }
            }
        }

        tri_data_t &next_best_tri = triangles[next_best_index];

        for (unsigned int indice : next_best_tri.indices) {
            out_indices[out_index++] = indice;

            vtx_data_t &v = vertices[indice];
            v.active_tris_count--;
            for (uint32_t k = 0; k < v.ref_count; k++) {
                if (v.tris[k] == next_best_index) {
                    v.tris[k] = -1;
                    break;
                }
            }

            use_vertex(lru_cache, indice);
        }

        next_best_tri.is_in_list = true;

        std::vector<uint32_t> tris_to_update;
        enforce_size(lru_cache, &vertices[0], MaxSizeVertexCache, tris_to_update);

        next_best_score = -1.0f;
        next_best_index = -1;

        for (const uint32_t ti : tris_to_update) {
            tri_data_t &tri = triangles[ti];

            if (!tri.is_in_list) {
                tri.score =
                    vertices[tri.indices[0]].score + vertices[tri.indices[1]].score + vertices[tri.indices[2]].score;

                if (tri.score > next_best_score) {
                    if (next_best_score > next_next_best_score) {
                        next_next_best_index = next_best_index;
                        next_next_best_score = next_best_score;
                    }
                    next_best_index = ti;
                    next_best_score = tri.score;
                }

                if (tri.score > next_next_best_score) {
                    next_next_best_index = ti;
                    next_next_best_score = tri.score;
                }
            }
        }

        if (next_best_index == -1 && next_next_best_index != -1) {
            if (!triangles[next_next_best_index].is_in_list) {
                next_best_index = next_next_best_index;
                next_best_score = next_next_best_score;
            }

            next_next_best_index = -1;
            next_next_best_score = -1.0f;
        }
    }
}

void Ren::ComputeTextureBasis(std::vector<vertex_t> &vertices, std::vector<uint32_t> index_groups[],
                              const int groups_count) {
    const float flt_eps = 0.0000001f;

    std::vector<std::array<uint32_t, 3>> twin_verts(vertices.size(), {0, 0, 0});
    std::vector<Vec3f> binormals(vertices.size());
    for (int grp = 0; grp < groups_count; grp++) {
        std::vector<uint32_t> &indices = index_groups[grp];
        for (size_t i = 0; i < indices.size(); i += 3) {
            vertex_t *v0 = &vertices[indices[i + 0]];
            vertex_t *v1 = &vertices[indices[i + 1]];
            vertex_t *v2 = &vertices[indices[i + 2]];

            Vec3f &b0 = binormals[indices[i + 0]];
            Vec3f &b1 = binormals[indices[i + 1]];
            Vec3f &b2 = binormals[indices[i + 2]];

            const Vec3f dp1 = MakeVec3(v1->p) - MakeVec3(v0->p);
            const Vec3f dp2 = MakeVec3(v2->p) - MakeVec3(v0->p);

            const Vec2f dt1 = MakeVec2(v1->t[0]) - MakeVec2(v0->t[0]);
            const Vec2f dt2 = MakeVec2(v2->t[0]) - MakeVec2(v0->t[0]);

            Vec3f tangent, binormal;

            const float det = dt1[0] * dt2[1] - dt1[1] * dt2[0];
            if (std::abs(det) > flt_eps) {
                const float inv_det = 1.0f / det;
                tangent = (dp1 * dt2[1] - dp2 * dt1[1]) * inv_det;
                binormal = (dp2 * dt1[0] - dp1 * dt2[0]) * inv_det;
            } else {
                const Vec3f plane_N = Cross(dp1, dp2);
                tangent = Vec3f{0.0f, 1.0f, 0.0f};
                if (std::abs(plane_N[0]) <= std::abs(plane_N[1]) && std::abs(plane_N[0]) <= std::abs(plane_N[2])) {
                    tangent = Vec3f{1.0f, 0.0f, 0.0f};
                } else if (std::abs(plane_N[2]) <= std::abs(plane_N[0]) &&
                           std::abs(plane_N[2]) <= std::abs(plane_N[1])) {
                    tangent = Vec3f{0.0f, 0.0f, 1.0f};
                }

                binormal = Normalize(Cross(Vec3f(plane_N), tangent));
                tangent = Normalize(Cross(Vec3f(plane_N), binormal));
            }

            int i1 = (v0->b[0] * tangent[0] + v0->b[1] * tangent[1] + v0->b[2] * tangent[2]) < 0;
            int i2 = 2 * (b0[0] * binormal[0] + b0[1] * binormal[1] + b0[2] * binormal[2] < 0);

            if (i1 || i2) {
                uint32_t index = twin_verts[indices[i + 0]][i1 + i2 - 1];
                if (index == 0) {
                    index = (uint32_t)(vertices.size());
                    vertices.push_back(*v0);
                    memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                    twin_verts[indices[i + 0]][i1 + i2 - 1] = index;

                    v1 = &vertices[indices[i + 1]];
                    v2 = &vertices[indices[i + 2]];
                }
                indices[i] = index;
                v0 = &vertices[index];
            } else {
                b0 = binormal;
            }

            v0->b[0] += tangent[0];
            v0->b[1] += tangent[1];
            v0->b[2] += tangent[2];

            i1 = v1->b[0] * tangent[0] + v1->b[1] * tangent[1] + v1->b[2] * tangent[2] < 0;
            i2 = 2 * (b1[0] * binormal[0] + b1[1] * binormal[1] + b1[2] * binormal[2] < 0);

            if (i1 || i2) {
                uint32_t index = twin_verts[indices[i + 1]][i1 + i2 - 1];
                if (index == 0) {
                    index = (uint32_t)(vertices.size());
                    vertices.push_back(*v1);
                    memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                    twin_verts[indices[i + 1]][i1 + i2 - 1] = index;

                    v0 = &vertices[indices[i + 0]];
                    v2 = &vertices[indices[i + 2]];
                }
                indices[i + 1] = index;
                v1 = &vertices[index];
            } else {
                b1 = binormal;
            }

            v1->b[0] += tangent[0];
            v1->b[1] += tangent[1];
            v1->b[2] += tangent[2];

            i1 = v2->b[0] * tangent[0] + v2->b[1] * tangent[1] + v2->b[2] * tangent[2] < 0;
            i2 = 2 * (b2[0] * binormal[0] + b2[1] * binormal[1] + b2[2] * binormal[2] < 0);

            if (i1 || i2) {
                uint32_t index = twin_verts[indices[i + 2]][i1 + i2 - 1];
                if (index == 0) {
                    index = (uint32_t)(vertices.size());
                    vertices.push_back(*v2);
                    memset(&vertices.back().b[0], 0, 3 * sizeof(float));
                    twin_verts[indices[i + 2]][i1 + i2 - 1] = index;

                    v0 = &vertices[indices[i + 0]];
                    v1 = &vertices[indices[i + 1]];
                }
                indices[i + 2] = index;
                v2 = &vertices[index];
            } else {
                b2 = binormal;
            }

            v2->b[0] += tangent[0];
            v2->b[1] += tangent[1];
            v2->b[2] += tangent[2];
        }
    }

    for (vertex_t &v : vertices) {
        if (std::abs(v.b[0]) > flt_eps || std::abs(v.b[1]) > flt_eps || std::abs(v.b[2]) > flt_eps) {
            const Vec3f tangent = MakeVec3(v.b);
            Vec3f binormal = Cross(MakeVec3(v.n), tangent);
            float l = Length(binormal);
            if (l > flt_eps) {
                binormal /= l;
                memcpy(&v.b[0], &binormal[0], 3 * sizeof(float));
            }
        }
    }
}

//	Classic Perlin 3D Noise
//	by Stefan Gustavson
//
float Ren::PerlinNoise(const Ren::Vec4f &P) {
    Vec4f Pi0 = Floor(P);          // Integer part for indexing
    Vec4f Pi1 = Pi0 + Vec4f{1.0f}; // Integer part + 1
    Pi0 = Mod(Pi0, Vec4f{289.0f});
    Pi1 = Mod(Pi1, Vec4f{289.0f});
    const Vec4f Pf0 = Fract(P);         // Fractional part for interpolation
    const Vec4f Pf1 = Pf0 - Vec4f{1.0}; // Fractional part - 1.0
    const auto ix = Vec4f{Pi0[0], Pi1[0], Pi0[0], Pi1[0]};
    const auto iy = Vec4f{Pi0[1], Pi0[1], Pi1[1], Pi1[1]};
    const auto iz0 = Vec4f{Pi0[2]};
    const auto iz1 = Vec4f{Pi1[2]};
    const auto iw0 = Vec4f{Pi0[3]};
    const auto iw1 = Vec4f{Pi1[3]};

    const Vec4f ixy = permute(permute(ix) + iy);
    const Vec4f ixy0 = permute(ixy + iz0);
    const Vec4f ixy1 = permute(ixy + iz1);
    const Vec4f ixy00 = permute(ixy0 + iw0);
    const Vec4f ixy01 = permute(ixy0 + iw1);
    const Vec4f ixy10 = permute(ixy1 + iw0);
    const Vec4f ixy11 = permute(ixy1 + iw1);

    Vec4f gx00 = ixy00 / 7.0f;
    Vec4f gy00 = Floor(gx00) / 7.0;
    Vec4f gz00 = Floor(gy00) / 6.0;
    gx00 = Fract(gx00) - Vec4f{0.5f};
    gy00 = Fract(gy00) - Vec4f{0.5f};
    gz00 = Fract(gz00) - Vec4f{0.5f};
    Vec4f gw00 = Vec4f{0.75} - Abs(gx00) - Abs(gy00) - Abs(gz00);
    Vec4f sw00 = Step(gw00, Vec4f{0.0f});
    gx00 -= sw00 * (Step(Vec4f{0.0f}, gx00) - Vec4f{0.5f});
    gy00 -= sw00 * (Step(Vec4f{0.0f}, gy00) - Vec4f{0.5f});

    Vec4f gx01 = ixy01 / 7.0f;
    Vec4f gy01 = Floor(gx01) / 7.0f;
    Vec4f gz01 = Floor(gy01) / 6.0f;
    gx01 = Fract(gx01) - Vec4f{0.5};
    gy01 = Fract(gy01) - Vec4f{0.5};
    gz01 = Fract(gz01) - Vec4f{0.5};
    Vec4f gw01 = Vec4f{0.75f} - Abs(gx01) - Abs(gy01) - Abs(gz01);
    Vec4f sw01 = Step(gw01, Vec4f{0.0f});
    gx01 -= sw01 * (Step(Vec4f{0.0f}, gx01) - Vec4f{0.5f});
    gy01 -= sw01 * (Step(Vec4f{0.0f}, gy01) - Vec4f{0.5f});

    Vec4f gx10 = ixy10 / 7.0f;
    Vec4f gy10 = Floor(gx10) / 7.0f;
    Vec4f gz10 = Floor(gy10) / 6.0f;
    gx10 = Fract(gx10) - Vec4f{0.5};
    gy10 = Fract(gy10) - Vec4f{0.5};
    gz10 = Fract(gz10) - Vec4f{0.5};
    Vec4f gw10 = Vec4f{0.75f} - Abs(gx10) - Abs(gy10) - Abs(gz10);
    Vec4f sw10 = Step(gw10, Vec4f{0.0f});
    gx10 -= sw10 * (Step(Vec4f{0.0}, gx10) - Vec4f{0.5});
    gy10 -= sw10 * (Step(Vec4f{0.0}, gy10) - Vec4f{0.5});

    Vec4f gx11 = ixy11 / 7.0f;
    Vec4f gy11 = Floor(gx11) / 7.0f;
    Vec4f gz11 = Floor(gy11) / 6.0f;
    gx11 = Fract(gx11) - Vec4f{0.5f};
    gy11 = Fract(gy11) - Vec4f{0.5f};
    gz11 = Fract(gz11) - Vec4f{0.5f};
    Vec4f gw11 = Vec4f{0.75f} - Abs(gx11) - Abs(gy11) - Abs(gz11);
    Vec4f sw11 = Step(gw11, Vec4f{0.0f});
    gx11 -= sw11 * (Step(Vec4f{0.0f}, gx11) - Vec4f{0.5f});
    gy11 -= sw11 * (Step(Vec4f{0.0f}, gy11) - Vec4f{0.5f});

    auto g0000 = Vec4f{gx00[0], gy00[0], gz00[0], gw00[0]};
    auto g1000 = Vec4f{gx00[1], gy00[1], gz00[1], gw00[1]};
    auto g0100 = Vec4f{gx00[2], gy00[2], gz00[2], gw00[2]};
    auto g1100 = Vec4f{gx00[3], gy00[3], gz00[3], gw00[3]};
    auto g0010 = Vec4f{gx10[0], gy10[0], gz10[0], gw10[0]};
    auto g1010 = Vec4f{gx10[1], gy10[1], gz10[1], gw10[1]};
    auto g0110 = Vec4f{gx10[2], gy10[2], gz10[2], gw10[2]};
    auto g1110 = Vec4f{gx10[3], gy10[3], gz10[3], gw10[3]};
    auto g0001 = Vec4f{gx01[0], gy01[0], gz01[0], gw01[0]};
    auto g1001 = Vec4f{gx01[1], gy01[1], gz01[1], gw01[1]};
    auto g0101 = Vec4f{gx01[2], gy01[2], gz01[2], gw01[2]};
    auto g1101 = Vec4f{gx01[3], gy01[3], gz01[3], gw01[3]};
    auto g0011 = Vec4f{gx11[0], gy11[0], gz11[0], gw11[0]};
    auto g1011 = Vec4f{gx11[1], gy11[1], gz11[1], gw11[1]};
    auto g0111 = Vec4f{gx11[2], gy11[2], gz11[2], gw11[2]};
    auto g1111 = Vec4f{gx11[3], gy11[3], gz11[3], gw11[3]};

    const Vec4f norm00 =
        taylor_inv_sqrt(Vec4f{Dot(g0000, g0000), Dot(g0100, g0100), Dot(g1000, g1000), Dot(g1100, g1100)});
    g0000 *= norm00[0];
    g0100 *= norm00[1];
    g1000 *= norm00[2];
    g1100 *= norm00[3];

    const Vec4f norm01 =
        taylor_inv_sqrt(Vec4f{Dot(g0001, g0001), Dot(g0101, g0101), Dot(g1001, g1001), Dot(g1101, g1101)});
    g0001 *= norm01[0];
    g0101 *= norm01[1];
    g1001 *= norm01[2];
    g1101 *= norm01[3];

    const Vec4f norm10 =
        taylor_inv_sqrt(Vec4f{Dot(g0010, g0010), Dot(g0110, g0110), Dot(g1010, g1010), Dot(g1110, g1110)});
    g0010 *= norm10[0];
    g0110 *= norm10[1];
    g1010 *= norm10[2];
    g1110 *= norm10[3];

    const Vec4f norm11 =
        taylor_inv_sqrt(Vec4f{Dot(g0011, g0011), Dot(g0111, g0111), Dot(g1011, g1011), Dot(g1111, g1111)});
    g0011 *= norm11[0];
    g0111 *= norm11[1];
    g1011 *= norm11[2];
    g1111 *= norm11[3];

    const float n0000 = Dot(g0000, Pf0);
    const float n1000 = Dot(g1000, Vec4f{Pf1[0], Pf0[1], Pf0[2], Pf0[3]});
    const float n0100 = Dot(g0100, Vec4f{Pf0[0], Pf1[1], Pf0[2], Pf0[3]});
    const float n1100 = Dot(g1100, Vec4f{Pf1[0], Pf1[1], Pf0[2], Pf0[3]});
    const float n0010 = Dot(g0010, Vec4f{Pf0[0], Pf0[1], Pf1[2], Pf0[3]});
    const float n1010 = Dot(g1010, Vec4f{Pf1[0], Pf0[1], Pf1[2], Pf0[3]});
    const float n0110 = Dot(g0110, Vec4f{Pf0[0], Pf1[1], Pf1[2], Pf0[3]});
    const float n1110 = Dot(g1110, Vec4f{Pf1[0], Pf1[1], Pf1[2], Pf0[3]});
    const float n0001 = Dot(g0001, Vec4f{Pf0[0], Pf0[1], Pf0[2], Pf1[3]});
    const float n1001 = Dot(g1001, Vec4f{Pf1[0], Pf0[1], Pf0[2], Pf1[3]});
    const float n0101 = Dot(g0101, Vec4f{Pf0[0], Pf1[1], Pf0[2], Pf1[3]});
    const float n1101 = Dot(g1101, Vec4f{Pf1[0], Pf1[1], Pf0[2], Pf1[3]});
    const float n0011 = Dot(g0011, Vec4f{Pf0[0], Pf0[1], Pf1[2], Pf1[3]});
    const float n1011 = Dot(g1011, Vec4f{Pf1[0], Pf0[1], Pf1[2], Pf1[3]});
    const float n0111 = Dot(g0111, Vec4f{Pf0[0], Pf1[1], Pf1[2], Pf1[3]});
    const float n1111 = Dot(g1111, Pf1);

    const Vec4f fade_xyzw = fade(Pf0);
    const Vec4f n_0w = Mix(Vec4f{n0000, n1000, n0100, n1100}, Vec4f{n0001, n1001, n0101, n1101}, fade_xyzw[3]);
    const Vec4f n_1w = Mix(Vec4f{n0010, n1010, n0110, n1110}, Vec4f{n0011, n1011, n0111, n1111}, fade_xyzw[3]);
    const Vec4f n_zw = Mix(n_0w, n_1w, fade_xyzw[2]);
    const Vec2f n_yzw = Mix(Vec2f{n_zw[0], n_zw[1]}, Vec2f{n_zw[2], n_zw[3]}, fade_xyzw[1]);
    const float n_xyzw = Mix(n_yzw[0], n_yzw[1], fade_xyzw[0]);
    return 2.2f * n_xyzw;
}

// Classic Perlin noise, periodic version
float Ren::PerlinNoise(const Ren::Vec4f &P, const Ren::Vec4f &rep) {
    const Vec4f Pi0 = Mod(Floor(P), rep);          // Integer part modulo rep
    const Vec4f Pi1 = Mod(Pi0 + Vec4f{1.0f}, rep); // Integer part + 1 mod rep
    const Vec4f Pf0 = Fract(P);                    // Fractional part for interpolation
    const Vec4f Pf1 = Pf0 - Vec4f{1.0f};           // Fractional part - 1.0
    const Vec4f ix = Vec4f{Pi0[0], Pi1[0], Pi0[0], Pi1[0]};
    const Vec4f iy = Vec4f{Pi0[1], Pi0[1], Pi1[1], Pi1[1]};
    const Vec4f iz0 = Vec4f{Pi0[2]};
    const Vec4f iz1 = Vec4f{Pi1[2]};
    const Vec4f iw0 = Vec4f{Pi0[3]};
    const Vec4f iw1 = Vec4f{Pi1[3]};

    const Vec4f ixy = permute(permute(ix) + iy);
    const Vec4f ixy0 = permute(ixy + iz0);
    const Vec4f ixy1 = permute(ixy + iz1);
    const Vec4f ixy00 = permute(ixy0 + iw0);
    const Vec4f ixy01 = permute(ixy0 + iw1);
    const Vec4f ixy10 = permute(ixy1 + iw0);
    const Vec4f ixy11 = permute(ixy1 + iw1);

    Vec4f gx00 = ixy00 / 7.0f;
    Vec4f gy00 = Floor(gx00) / 7.0f;
    Vec4f gz00 = Floor(gy00) / 6.0f;
    gx00 = Fract(gx00) - Vec4f{0.5f};
    gy00 = Fract(gy00) - Vec4f{0.5f};
    gz00 = Fract(gz00) - Vec4f{0.5f};
    Vec4f gw00 = Vec4f{0.75f} - Abs(gx00) - Abs(gy00) - Abs(gz00);
    Vec4f sw00 = Step(gw00, Vec4f{0.0f});
    gx00 -= sw00 * (Step(Vec4f{0.0f}, gx00) - Vec4f{0.5f});
    gy00 -= sw00 * (Step(Vec4f{0.0f}, gy00) - Vec4f{0.5f});

    Vec4f gx01 = ixy01 / 7.0f;
    Vec4f gy01 = Floor(gx01) / 7.0f;
    Vec4f gz01 = Floor(gy01) / 6.0f;
    gx01 = Fract(gx01) - Vec4f{0.5f};
    gy01 = Fract(gy01) - Vec4f{0.5f};
    gz01 = Fract(gz01) - Vec4f{0.5f};
    Vec4f gw01 = Vec4f{0.75f} - Abs(gx01) - Abs(gy01) - Abs(gz01);
    Vec4f sw01 = Step(gw01, Vec4f{0.0f});
    gx01 -= sw01 * (Step(Vec4f{0.0f}, gx01) - Vec4f{0.5f});
    gy01 -= sw01 * (Step(Vec4f{0.0f}, gy01) - Vec4f{0.5f});

    Vec4f gx10 = ixy10 / 7.0f;
    Vec4f gy10 = Floor(gx10) / 7.0f;
    Vec4f gz10 = Floor(gy10) / 6.0f;
    gx10 = Fract(gx10) - Vec4f{0.5f};
    gy10 = Fract(gy10) - Vec4f{0.5f};
    gz10 = Fract(gz10) - Vec4f{0.5f};
    Vec4f gw10 = Vec4f{0.75f} - Abs(gx10) - Abs(gy10) - Abs(gz10);
    Vec4f sw10 = Step(gw10, Vec4f{0.0f});
    gx10 -= sw10 * (Step(Vec4f{0.0f}, gx10) - Vec4f{0.5});
    gy10 -= sw10 * (Step(Vec4f{0.0f}, gy10) - Vec4f{0.5});

    Vec4f gx11 = ixy11 / 7.0f;
    Vec4f gy11 = Floor(gx11) / 7.0f;
    Vec4f gz11 = Floor(gy11) / 6.0f;
    gx11 = Fract(gx11) - Vec4f{0.5f};
    gy11 = Fract(gy11) - Vec4f{0.5f};
    gz11 = Fract(gz11) - Vec4f{0.5f};
    Vec4f gw11 = Vec4f{0.75f} - Abs(gx11) - Abs(gy11) - Abs(gz11);
    Vec4f sw11 = Step(gw11, Vec4f{0.0f});
    gx11 -= sw11 * (Step(Vec4f{0.0f}, gx11) - Vec4f{0.5f});
    gy11 -= sw11 * (Step(Vec4f{0.0f}, gy11) - Vec4f{0.5f});

    auto g0000 = Vec4f(gx00[0], gy00[0], gz00[0], gw00[0]);
    auto g1000 = Vec4f(gx00[1], gy00[1], gz00[1], gw00[1]);
    auto g0100 = Vec4f(gx00[2], gy00[2], gz00[2], gw00[2]);
    auto g1100 = Vec4f(gx00[3], gy00[3], gz00[3], gw00[3]);
    auto g0010 = Vec4f(gx10[0], gy10[0], gz10[0], gw10[0]);
    auto g1010 = Vec4f(gx10[1], gy10[1], gz10[1], gw10[1]);
    auto g0110 = Vec4f(gx10[2], gy10[2], gz10[2], gw10[2]);
    auto g1110 = Vec4f(gx10[3], gy10[3], gz10[3], gw10[3]);
    auto g0001 = Vec4f(gx01[0], gy01[0], gz01[0], gw01[0]);
    auto g1001 = Vec4f(gx01[1], gy01[1], gz01[1], gw01[1]);
    auto g0101 = Vec4f(gx01[2], gy01[2], gz01[2], gw01[2]);
    auto g1101 = Vec4f(gx01[3], gy01[3], gz01[3], gw01[3]);
    auto g0011 = Vec4f(gx11[0], gy11[0], gz11[0], gw11[0]);
    auto g1011 = Vec4f(gx11[1], gy11[1], gz11[1], gw11[1]);
    auto g0111 = Vec4f(gx11[2], gy11[2], gz11[2], gw11[2]);
    auto g1111 = Vec4f(gx11[3], gy11[3], gz11[3], gw11[3]);

    const Vec4f norm00 =
        taylor_inv_sqrt(Vec4f{Dot(g0000, g0000), Dot(g0100, g0100), Dot(g1000, g1000), Dot(g1100, g1100)});
    g0000 *= norm00[0];
    g0100 *= norm00[1];
    g1000 *= norm00[2];
    g1100 *= norm00[3];

    const Vec4f norm01 =
        taylor_inv_sqrt(Vec4f{Dot(g0001, g0001), Dot(g0101, g0101), Dot(g1001, g1001), Dot(g1101, g1101)});
    g0001 *= norm01[0];
    g0101 *= norm01[1];
    g1001 *= norm01[2];
    g1101 *= norm01[3];

    const Vec4f norm10 =
        taylor_inv_sqrt(Vec4f{Dot(g0010, g0010), Dot(g0110, g0110), Dot(g1010, g1010), Dot(g1110, g1110)});
    g0010 *= norm10[0];
    g0110 *= norm10[1];
    g1010 *= norm10[2];
    g1110 *= norm10[3];

    const Vec4f norm11 =
        taylor_inv_sqrt(Vec4f{Dot(g0011, g0011), Dot(g0111, g0111), Dot(g1011, g1011), Dot(g1111, g1111)});
    g0011 *= norm11[0];
    g0111 *= norm11[1];
    g1011 *= norm11[2];
    g1111 *= norm11[3];

    const float n0000 = Dot(g0000, Pf0);
    const float n1000 = Dot(g1000, Vec4f{Pf1[0], Pf0[1], Pf0[2], Pf0[3]});
    const float n0100 = Dot(g0100, Vec4f{Pf0[0], Pf1[1], Pf0[2], Pf0[3]});
    const float n1100 = Dot(g1100, Vec4f{Pf1[0], Pf1[1], Pf0[2], Pf0[3]});
    const float n0010 = Dot(g0010, Vec4f{Pf0[0], Pf0[1], Pf1[2], Pf0[3]});
    const float n1010 = Dot(g1010, Vec4f{Pf1[0], Pf0[1], Pf1[2], Pf0[3]});
    const float n0110 = Dot(g0110, Vec4f{Pf0[0], Pf1[1], Pf1[2], Pf0[3]});
    const float n1110 = Dot(g1110, Vec4f{Pf1[0], Pf1[1], Pf1[2], Pf0[3]});
    const float n0001 = Dot(g0001, Vec4f{Pf0[0], Pf0[1], Pf0[2], Pf1[3]});
    const float n1001 = Dot(g1001, Vec4f{Pf1[0], Pf0[1], Pf0[2], Pf1[3]});
    const float n0101 = Dot(g0101, Vec4f{Pf0[0], Pf1[1], Pf0[2], Pf1[3]});
    const float n1101 = Dot(g1101, Vec4f{Pf1[0], Pf1[1], Pf0[2], Pf1[3]});
    const float n0011 = Dot(g0011, Vec4f{Pf0[0], Pf0[1], Pf1[2], Pf1[3]});
    const float n1011 = Dot(g1011, Vec4f{Pf1[0], Pf0[1], Pf1[2], Pf1[3]});
    const float n0111 = Dot(g0111, Vec4f{Pf0[0], Pf1[1], Pf1[2], Pf1[3]});
    const float n1111 = Dot(g1111, Pf1);

    const Vec4f fade_xyzw = fade(Pf0);
    const Vec4f n_0w = Mix(Vec4f{n0000, n1000, n0100, n1100}, Vec4f{n0001, n1001, n0101, n1101}, fade_xyzw[3]);
    const Vec4f n_1w = Mix(Vec4f{n0010, n1010, n0110, n1110}, Vec4f{n0011, n1011, n0111, n1111}, fade_xyzw[3]);
    const Vec4f n_zw = Mix(n_0w, n_1w, fade_xyzw[2]);
    const Vec2f n_yzw = Mix(Vec2f{n_zw[0], n_zw[1]}, Vec2f{n_zw[2], n_zw[3]}, fade_xyzw[1]);
    const float n_xyzw = Mix(n_yzw[0], n_yzw[1], fade_xyzw[0]);
    return 2.2f * n_xyzw;
}

//
// https://software.intel.com/sites/default/files/23/1d/324337_324337.pdf
//

namespace Ren {
template <int Channels> void Extract4x4Block_Ref(const uint8_t src[], const int stride, uint8_t dst[64]) {
    if (Channels == 4) {
        for (int j = 0; j < 4; j++) {
            memcpy(&dst[j * 4 * 4], src, 4 * 4);
            src += stride;
        }
    } else if (Channels == 3) {
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                memcpy(&dst[i * 4], &src[i * 3], 3);
            }
            dst += 4 * 4;
            src += stride;
        }
    }
}

template <int Channels>
void ExtractIncomplete4x4Block_Ref(const uint8_t src[], const int stride, const int blck_w, const int blck_h,
                                   uint8_t dst[64]) {
    if (Channels == 4) {
        for (int j = 0; j < blck_h; j++) {
            assert(blck_w <= 4);
            memcpy(&dst[0], src, 4 * blck_w);
            for (int i = blck_w; i < 4; i++) {
                memcpy(&dst[i * 4], &dst[(blck_w - 1) * 4], 4);
            }
            dst += 4 * 4;
            src += stride;
        }
    } else if (Channels == 3) {
        for (int j = 0; j < blck_h; j++) {
            for (int i = 0; i < blck_w; i++) {
                memcpy(&dst[i * 4], &src[i * 3], 3);
            }
            for (int i = blck_w; i < 4; i++) {
                memcpy(&dst[i * 4], &dst[(blck_w - 1) * 4], 4);
            }
            dst += 4 * 4;
            src += stride;
        }
    }
    uint8_t *dst2 = dst - 4 * 4;
    for (int j = blck_h; j < 4; j++) {
        memcpy(dst, dst2, 4 * 4);
        dst += 4 * 4;
    }
}

// WARNING: Reads 4 bytes outside of block!
template <int Channels> void Extract4x4Block_SSSE3(const uint8_t src[], int stride, uint8_t dst[64]);

force_inline int ColorDistance(const uint8_t c1[3], const uint8_t c2[3]) {
    // euclidean distance
    return (c1[0] - c2[0]) * (c1[0] - c2[0]) + (c1[1] - c2[1]) * (c1[1] - c2[1]) + (c1[2] - c2[2]) * (c1[2] - c2[2]);
}

force_inline int ColorLumaApprox(const uint8_t color[3]) { return int(color[0] + color[1] * 2 + color[2]); }

force_inline uint16_t rgb888_to_rgb565(const uint8_t color[3]) {
    return ((color[0] >> 3) << 11) | ((color[1] >> 2) << 5) | (color[2] >> 3);
}

force_inline void swap_rgb(uint8_t c1[3], uint8_t c2[3]) {
    uint8_t tm[3];
    memcpy(tm, c1, 3);
    memcpy(c1, c2, 3);
    memcpy(c2, tm, 3);
}

void GetMinMaxColorByDistance(const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]) {
    int max_dist = -1;

    for (int i = 0; i < 64 - 4; i += 4) {
        for (int j = i + 4; j < 64; j += 4) {
            const int dist = ColorDistance(&block[i], &block[j]);
            if (dist > max_dist) {
                max_dist = dist;
                memcpy(min_color, &block[i], 3);
                memcpy(max_color, &block[j], 3);
            }
        }
    }

    if (rgb888_to_rgb565(max_color) < rgb888_to_rgb565(min_color)) {
        swap_rgb(min_color, max_color);
    }
}

void GetMinMaxColorByLuma(const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]) {
    int max_luma = -1, min_luma = std::numeric_limits<int>::max();

    for (int i = 0; i < 16; i++) {
        const int luma = ColorLumaApprox(&block[i * 4]);
        if (luma > max_luma) {
            memcpy(max_color, &block[i * 4], 3);
            max_luma = luma;
        }
        if (luma < min_luma) {
            memcpy(min_color, &block[i * 4], 3);
            min_luma = luma;
        }
    }

    if (rgb888_to_rgb565(max_color) < rgb888_to_rgb565(min_color)) {
        swap_rgb(min_color, max_color);
    }
}

template <bool UseAlpha = false, bool Is_YCoCg = false>
void GetMinMaxColorByBBox_Ref(const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]) {
    min_color[0] = min_color[1] = min_color[2] = min_color[3] = 255;
    max_color[0] = max_color[1] = max_color[2] = max_color[3] = 0;

    // clang-format off
    for (int i = 0; i < 16; i++) {
        if (block[i * 4 + 0] < min_color[0]) min_color[0] = block[i * 4 + 0];
        if (block[i * 4 + 1] < min_color[1]) min_color[1] = block[i * 4 + 1];
        if (block[i * 4 + 2] < min_color[2]) min_color[2] = block[i * 4 + 2];
        if (UseAlpha && block[i * 4 + 3] < min_color[3]) min_color[3] = block[i * 4 + 3];
        if (block[i * 4 + 0] > max_color[0]) max_color[0] = block[i * 4 + 0];
        if (block[i * 4 + 1] > max_color[1]) max_color[1] = block[i * 4 + 1];
        if (block[i * 4 + 2] > max_color[2]) max_color[2] = block[i * 4 + 2];
        if (UseAlpha && block[i * 4 + 3] > max_color[3]) max_color[3] = block[i * 4 + 3];
    }
    // clang-format on

    if (!Is_YCoCg) {
        // offset bbox inside by 1/16 of it's dimentions, this improves MSR (???)
        const uint8_t inset[] = {
            uint8_t((max_color[0] - min_color[0]) / 16), uint8_t((max_color[1] - min_color[1]) / 16),
            uint8_t((max_color[2] - min_color[2]) / 16), uint8_t((max_color[3] - min_color[3]) / 32)};

        min_color[0] = (min_color[0] + inset[0] <= 255) ? min_color[0] + inset[0] : 255;
        min_color[1] = (min_color[1] + inset[1] <= 255) ? min_color[1] + inset[1] : 255;
        min_color[2] = (min_color[2] + inset[2] <= 255) ? min_color[2] + inset[2] : 255;
        if (UseAlpha) {
            min_color[3] = (min_color[3] + inset[3] <= 255) ? min_color[3] + inset[3] : 255;
        }

        max_color[0] = (max_color[0] >= inset[0]) ? max_color[0] - inset[0] : 0;
        max_color[1] = (max_color[1] >= inset[1]) ? max_color[1] - inset[1] : 0;
        max_color[2] = (max_color[2] >= inset[2]) ? max_color[2] - inset[2] : 0;
        if (UseAlpha) {
            max_color[3] = (max_color[3] >= inset[3]) ? max_color[3] - inset[3] : 0;
        }
    }
}

template <bool UseAlpha = false, bool Is_YCoCg = false>
void GetMinMaxColorByBBox_SSE2(const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]);

void InsetYCoCgBBox_Ref(uint8_t min_color[4], uint8_t max_color[4]) {
    const int inset[] = {(max_color[0] - min_color[0]) - ((1 << (4 - 1)) - 1),
                         (max_color[1] - min_color[1]) - ((1 << (4 - 1)) - 1), 0,
                         (max_color[3] - min_color[3]) - ((1 << (5 - 1)) - 1)};

    int mini[4], maxi[4];

    mini[0] = ((min_color[0] * 16) + inset[0]) / 16;
    mini[1] = ((min_color[1] * 16) + inset[1]) / 16;
    mini[3] = ((min_color[3] * 32) + inset[3]) / 32;

    maxi[0] = ((max_color[0] * 16) - inset[0]) / 16;
    maxi[1] = ((max_color[1] * 16) - inset[1]) / 16;
    maxi[3] = ((max_color[3] * 32) - inset[3]) / 32;

    mini[0] = (mini[0] >= 0) ? mini[0] : 0;
    mini[1] = (mini[1] >= 0) ? mini[1] : 0;
    mini[3] = (mini[3] >= 0) ? mini[3] : 0;

    maxi[0] = (maxi[0] <= 255) ? maxi[0] : 255;
    maxi[1] = (maxi[1] <= 255) ? maxi[1] : 255;
    maxi[3] = (maxi[3] <= 255) ? maxi[3] : 255;

    min_color[0] = (mini[0] & 0b11111000) | (mini[0] >> 5u);
    min_color[1] = (mini[1] & 0b11111100) | (mini[1] >> 6u);
    min_color[3] = mini[3];

    max_color[0] = (maxi[0] & 0b11111000) | (maxi[0] >> 5u);
    max_color[1] = (maxi[1] & 0b11111100) | (maxi[1] >> 6u);
    max_color[3] = maxi[3];
}

void InsetYCoCgBBox_SSE2(uint8_t min_color[4], uint8_t max_color[4]);

void SelectYCoCgDiagonal_Ref(const uint8_t block[64], uint8_t min_color[3], uint8_t max_color[3]) {
    const uint8_t mid0 = (int(min_color[0]) + max_color[0] + 1) / 2;
    const uint8_t mid1 = (int(min_color[1]) + max_color[1] + 1) / 2;

#if 0 // use covariance
    int covariance = 0;
    for (int i = 0; i < 16; i++) {
        const int b0 = block[i * 4 + 0] - mid0;
        const int b1 = block[i * 4 + 1] - mid1;
        covariance += (b0 * b1);
    }

    // flip diagonal
    if (covariance) {
        const uint8_t t = min_color[1];
        min_color[1] = max_color[1];
        max_color[1] = t;
    }
#else // use sign only
    uint8_t side = 0;
    for (int i = 0; i < 16; i++) {
        const uint8_t b0 = block[i * 4 + 0] >= mid0;
        const uint8_t b1 = block[i * 4 + 1] >= mid1;
        side += (b0 ^ b1);
    }

    uint8_t mask = -(side > 8);

    uint8_t c0 = min_color[1];
    uint8_t c1 = max_color[1];

    c0 ^= c1 ^= mask &= c0 ^= c1; // WTF?

    min_color[1] = c0;
    max_color[1] = c1;
#endif
}

void SelectYCoCgDiagonal_SSE2(const uint8_t block[64], uint8_t min_color[3], uint8_t max_color[3]);

void ScaleYCoCg_Ref(uint8_t block[64], uint8_t min_color[3], uint8_t max_color[3]) {
    int m0 = _ABS(min_color[0] - 128);
    int m1 = _ABS(min_color[1] - 128);
    int m2 = _ABS(max_color[0] - 128);
    int m3 = _ABS(max_color[1] - 128);

    // clang-format off
    if (m1 > m0) m0 = m1;
    if (m3 > m2) m2 = m3;
    if (m2 > m0) m0 = m2;
    // clang-format on

    const int s0 = 128 / 2 - 1;
    const int s1 = 128 / 4 - 1;

    const int mask0 = -(m0 <= s0);
    const int mask1 = -(m0 <= s1);
    const int scale = 1 + (1 & mask0) + (2 & mask1);

    min_color[0] = (min_color[0] - 128) * scale + 128;
    min_color[1] = (min_color[1] - 128) * scale + 128;
    min_color[2] = (scale - 1) * 8;

    max_color[0] = (max_color[0] - 128) * scale + 128;
    max_color[1] = (max_color[1] - 128) * scale + 128;
    max_color[2] = (scale - 1) * 8;

    for (int i = 0; i < 16; i++) {
        block[i * 4 + 0] = (block[i * 4 + 0] - 128) * scale + 128;
        block[i * 4 + 1] = (block[i * 4 + 1] - 128) * scale + 128;
    }
}

void ScaleYCoCg_SSE2(uint8_t block[64], uint8_t min_color[3], uint8_t max_color[3]);

force_inline void push_u8(const uint8_t v, uint8_t *&out_data) { (*out_data++) = v; }

force_inline void push_u16(const uint16_t v, uint8_t *&out_data) {
    (*out_data++) = (v >> 0) & 0xFF;
    (*out_data++) = (v >> 8) & 0xFF;
}

force_inline void push_u32(const uint32_t v, uint8_t *&out_data) {
    (*out_data++) = (v >> 0) & 0xFF;
    (*out_data++) = (v >> 8) & 0xFF;
    (*out_data++) = (v >> 16) & 0xFF;
    (*out_data++) = (v >> 24) & 0xFF;
}

void EmitColorIndices_Ref(const uint8_t block[64], const uint8_t min_color[3], const uint8_t max_color[3],
                          uint8_t *&out_data) {
    uint8_t colors[4][4];

    // get two initial colors (as if they were converted to rgb565 and back
    // note: the last 3 bits are replicated from the first 3 bits (???)
    colors[0][0] = (max_color[0] & 0b11111000) | (max_color[0] >> 5u);
    colors[0][1] = (max_color[1] & 0b11111100) | (max_color[1] >> 6u);
    colors[0][2] = (max_color[2] & 0b11111000) | (max_color[2] >> 5u);
    colors[1][0] = (min_color[0] & 0b11111000) | (min_color[0] >> 5u);
    colors[1][1] = (min_color[1] & 0b11111100) | (min_color[1] >> 6u);
    colors[1][2] = (min_color[2] & 0b11111000) | (min_color[2] >> 5u);
    // get two interpolated colors
    colors[2][0] = (2 * colors[0][0] + 1 * colors[1][0]) / 3;
    colors[2][1] = (2 * colors[0][1] + 1 * colors[1][1]) / 3;
    colors[2][2] = (2 * colors[0][2] + 1 * colors[1][2]) / 3;
    colors[3][0] = (1 * colors[0][0] + 2 * colors[1][0]) / 3;
    colors[3][1] = (1 * colors[0][1] + 2 * colors[1][1]) / 3;
    colors[3][2] = (1 * colors[0][2] + 2 * colors[1][2]) / 3;

    // division by 3 can be 'emulated' with:
    // y = (1 << 16) / 3 + 1
    // x = (x * y) >> 16          -->      pmulhw x, y

    // find best ind for each pixel in a block
    uint32_t result_indices = 0;

#if 0   // use euclidian distance (slower)
        uint32_t palette_indices[16];
        for (int i = 0; i < 16; i++) {
            uint32_t min_dist = std::numeric_limits<uint32_t>::max();
            for (int j = 0; j < 4; j++) {
                const uint32_t dist = ColorDistance(&block[i * 4], &colors[j][0]);
                if (dist < min_dist) {
                    palette_indices[i] = j;
                    min_dist = dist;
                }
            }
        }

        // pack ind in 2 bits each
        for (int i = 0; i < 16; i++) {
            result_indices |= (palette_indices[i] << uint32_t(i * 2));
        }
#elif 1 // use absolute differences (faster)
    for (int i = 15; i >= 0; i--) {
        const int c0 = block[i * 4 + 0];
        const int c1 = block[i * 4 + 1];
        const int c2 = block[i * 4 + 2];

        const int d0 = _ABS(colors[0][0] - c0) + _ABS(colors[0][1] - c1) + _ABS(colors[0][2] - c2);
        const int d1 = _ABS(colors[1][0] - c0) + _ABS(colors[1][1] - c1) + _ABS(colors[1][2] - c2);
        const int d2 = _ABS(colors[2][0] - c0) + _ABS(colors[2][1] - c1) + _ABS(colors[2][2] - c2);
        const int d3 = _ABS(colors[3][0] - c0) + _ABS(colors[3][1] - c1) + _ABS(colors[3][2] - c2);

        const int b0 = d0 > d3;
        const int b1 = d1 > d2;
        const int b2 = d0 > d2;
        const int b3 = d1 > d3;
        const int b4 = d2 > d3;

        const int x0 = b1 & b2;
        const int x1 = b0 & b3;
        const int x2 = b0 & b4;

        result_indices |= (x2 | ((x0 | x1) << 1)) << (i * 2);
    }
#endif

    push_u32(result_indices, out_data);
}

void EmitColorIndices_SSE2(const uint8_t block[64], const uint8_t min_color[4], const uint8_t max_color[4],
                           uint8_t *&out_data);

void EmitAlphaIndices_Ref(const uint8_t block[64], const uint8_t min_alpha, const uint8_t max_alpha,
                          uint8_t *&out_data) {
    uint8_t ind[16];

#if 0 // simple version
    const uint8_t alphas[8] = {max_alpha,
                               min_alpha,
                               uint8_t((6 * max_alpha + 1 * min_alpha) / 7),
                               uint8_t((5 * max_alpha + 2 * min_alpha) / 7),
                               uint8_t((4 * max_alpha + 3 * min_alpha) / 7),
                               uint8_t((3 * max_alpha + 4 * min_alpha) / 7),
                               uint8_t((2 * max_alpha + 5 * min_alpha) / 7),
                               uint8_t((1 * max_alpha + 6 * min_alpha) / 7)};
    for (int i = 0; i < 16; i++) {
        int min_dist = std::numeric_limits<int>::max();
        const uint8_t a = block[i * 4 + 3];
        for (int j = 0; j < 8; j++) {
            const int dist = _ABS(a - alphas[j]);
            if (dist < min_dist) {
                ind[i] = j;
                min_dist = dist;
            }
        }
    }
#else // parallel-friendly version
    const uint8_t half_step = (max_alpha - min_alpha) / (2 * 7);

    // division by 14 and 7 can be 'emulated' with:
    // y = (1 << 16) / 14 + 1
    // x = (x * y) >> 16          -->      pmulhw x, y

    const uint8_t ab1 = min_alpha + half_step;
    const uint8_t ab2 = (6 * max_alpha + 1 * min_alpha) / 7 + half_step;
    const uint8_t ab3 = (5 * max_alpha + 2 * min_alpha) / 7 + half_step;
    const uint8_t ab4 = (4 * max_alpha + 3 * min_alpha) / 7 + half_step;
    const uint8_t ab5 = (3 * max_alpha + 4 * min_alpha) / 7 + half_step;
    const uint8_t ab6 = (2 * max_alpha + 5 * min_alpha) / 7 + half_step;
    const uint8_t ab7 = (1 * max_alpha + 6 * min_alpha) / 7 + half_step;

    for (int i = 0; i < 16; i++) {
        const uint8_t a = block[i * 4 + 3];

        const int b1 = (a <= ab1);
        const int b2 = (a <= ab2);
        const int b3 = (a <= ab3);
        const int b4 = (a <= ab4);
        const int b5 = (a <= ab5);
        const int b6 = (a <= ab6);
        const int b7 = (a <= ab7);

        // x <= y can be emulated with min(x, y) == x

        const int ndx = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + 1) & 0b00000111;
        ind[i] = ndx ^ (2 > ndx);
    }
#endif

    // Write indices 3 bit each (48 = 4x8 in total)
    // [ 2][ 2][ 1][ 1][ 1][ 0][ 0][ 0]
    push_u8((ind[0] >> 0) | (ind[1] << 3) | (ind[2] << 6), out_data);
    // [ 5][ 4][ 4][ 4][ 3][ 3][ 3][ 2]
    push_u8((ind[2] >> 2) | (ind[3] << 1) | (ind[4] << 4) | (ind[5] << 7), out_data);
    // [ 7][ 7][ 7][ 6][ 6][ 6][ 5][ 5]
    push_u8((ind[5] >> 1) | (ind[6] << 2) | (ind[7] << 5), out_data);
    // [10][10][ 9][ 9][ 9][ 8][ 8][ 8]
    push_u8((ind[8] >> 0) | (ind[9] << 3) | (ind[10] << 6), out_data);
    // [13][12][12][12][11][11][11][10]
    push_u8((ind[10] >> 2) | (ind[11] << 1) | (ind[12] << 4) | (ind[13] << 7), out_data);
    // [15][15][15][14][14][14][13][13]
    push_u8((ind[13] >> 1) | (ind[14] << 2) | (ind[15] << 5), out_data);
}

void EmitAlphaIndices_SSE2(const uint8_t block[64], uint8_t min_alpha, uint8_t max_alpha, uint8_t *&out_data);

void EmitDXT1Block_Ref(const uint8_t block[64], uint8_t *&out_data) {
    uint8_t min_color[4], max_color[4];
    GetMinMaxColorByBBox_Ref(block, min_color, max_color);

    push_u16(rgb888_to_rgb565(max_color), out_data);
    push_u16(rgb888_to_rgb565(min_color), out_data);

    EmitColorIndices_Ref(block, min_color, max_color, out_data);
}

template <bool Is_YCoCg> void EmitDXT5Block_Ref(uint8_t block[64], uint8_t *&out_data) {
    uint8_t min_color[4], max_color[4];
    GetMinMaxColorByBBox_Ref<true /* UseAlpha */, Is_YCoCg>(block, min_color, max_color);
    if (Is_YCoCg) {
        ScaleYCoCg_Ref(block, min_color, max_color);
        InsetYCoCgBBox_Ref(min_color, max_color);
        SelectYCoCgDiagonal_Ref(block, min_color, max_color);
    }

    //
    // Write alpha block
    //

    push_u8(max_color[3], out_data);
    push_u8(min_color[3], out_data);

    EmitAlphaIndices_Ref(block, min_color[3], max_color[3], out_data);

    //
    // Write color block
    //

    push_u16(rgb888_to_rgb565(max_color), out_data);
    push_u16(rgb888_to_rgb565(min_color), out_data);

    EmitColorIndices_Ref(block, min_color, max_color, out_data);
}

#if !defined(__aarch64__)
void EmitDXT1Block_SSE2(const uint8_t block[64], uint8_t *&out_data) {
    alignas(16) uint8_t min_color[4], max_color[4];
    GetMinMaxColorByBBox_SSE2(block, min_color, max_color);

    push_u16(rgb888_to_rgb565(max_color), out_data);
    push_u16(rgb888_to_rgb565(min_color), out_data);

    EmitColorIndices_SSE2(block, min_color, max_color, out_data);
}

template <bool Is_YCoCg> void EmitDXT5Block_SSE2(uint8_t block[64], uint8_t *&out_data) {
    alignas(16) uint8_t min_color[4], max_color[4];
    GetMinMaxColorByBBox_SSE2<true /* UseAlpha */, Is_YCoCg>(block, min_color, max_color);
    if (Is_YCoCg) {
        ScaleYCoCg_SSE2(block, min_color, max_color);
        InsetYCoCgBBox_SSE2(min_color, max_color);
        SelectYCoCgDiagonal_SSE2(block, min_color, max_color);
    }

    //
    // Write alpha block
    //

    push_u8(max_color[3], out_data);
    push_u8(min_color[3], out_data);

    EmitAlphaIndices_SSE2(block, min_color[3], max_color[3], out_data);

    //
    // Write color block
    //

    push_u16(rgb888_to_rgb565(max_color), out_data);
    push_u16(rgb888_to_rgb565(min_color), out_data);

    EmitColorIndices_SSE2(block, min_color, max_color, out_data);
}
#endif

// clang-format off

const int BlockSize_DXT1 = 2 * sizeof(uint16_t) + sizeof(uint32_t);
//                         \_ low/high colors_/   \_ 16 x 2-bit _/

const int BlockSize_DXT5 = 2 * sizeof(uint8_t) + 6 * sizeof(uint8_t) +
//                         \_ low/high alpha_/     \_ 16 x 3-bit _/
                           2 * sizeof(uint16_t) + sizeof(uint32_t);
//                         \_ low/high colors_/   \_ 16 x 2-bit _/

// clang-format on

} // namespace Ren

int Ren::GetRequiredMemory_DXT1(const int w, const int h) { return BlockSize_DXT1 * ((w + 3) / 4) * ((h + 3) / 4); }

int Ren::GetRequiredMemory_DXT5(const int w, const int h) { return BlockSize_DXT5 * ((w + 3) / 4) * ((h + 3) / 4); }

template <int Channels>
void Ren::CompressImage_DXT1(const uint8_t img_src[], const int w, const int h, uint8_t img_dst[]) {
    alignas(16) uint8_t block[64] = {};
    uint8_t *p_out = img_dst;

    const int w_aligned = w - (w % 4);
    const int h_aligned = h - (h % 4);

#if !defined(__aarch64__)
    if (g_CpuFeatures.sse2_supported && g_CpuFeatures.ssse3_supported) {
        for (int j = 0; j < h_aligned; j += 4, img_src += 4 * w * Channels) {
            const int w_limited = (Channels == 3 && j == h_aligned - 4 && h_aligned == h) ? w_aligned - 4 : w_aligned;
            for (int i = 0; i < w_limited; i += 4) {
                Extract4x4Block_SSSE3<Channels>(&img_src[i * Channels], w * Channels, block);
                EmitDXT1Block_SSE2(block, p_out);
            }
            if (w_limited != w_aligned && w_aligned >= 4) {
                // process last block (avoid reading 4 bytes outside of range)
                Extract4x4Block_Ref<Channels>(&img_src[(w_aligned - 4) * Channels], w * Channels, block);
                EmitDXT1Block_SSE2(block, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<Channels>(&img_src[w_aligned * Channels], w * Channels, w % 4, 4, block);
                EmitDXT1Block_SSE2(block, p_out);
            }
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<Channels>(&img_src[i * Channels], w * Channels, _MIN(4, w - i), h % 4, block);
            EmitDXT1Block_SSE2(block, p_out);
        }
    } else
#endif
    {
        for (int j = 0; j < h_aligned; j += 4, img_src += 4 * w * Channels) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_Ref<Channels>(&img_src[i * Channels], w * Channels, block);
                EmitDXT1Block_Ref(block, p_out);
            }
            // process last column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<Channels>(&img_src[w_aligned * Channels], w * Channels, w % 4, 4, block);
                EmitDXT1Block_Ref(block, p_out);
            }
        }
        // process last row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<Channels>(&img_src[i * Channels], w * Channels, _MIN(4, w - i), h % 4, block);
            EmitDXT1Block_Ref(block, p_out);
        }
    }
}

template void Ren::CompressImage_DXT1<4 /* Channels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[]);
template void Ren::CompressImage_DXT1<3 /* Channels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[]);

template <bool Is_YCoCg>
void Ren::CompressImage_DXT5(const uint8_t img_src[], const int w, const int h, uint8_t img_dst[]) {
    alignas(16) uint8_t block[64] = {};
    uint8_t *p_out = img_dst;

    const int w_aligned = w - (w % 4);
    const int h_aligned = h - (h % 4);

#if !defined(__aarch64__)
    if (g_CpuFeatures.sse2_supported && g_CpuFeatures.ssse3_supported) {
        for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * 4) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_SSSE3<4 /* Channels */>(&img_src[i * 4], w * 4, block);
                EmitDXT5Block_SSE2<Is_YCoCg>(block, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<4 /* Channels */>(&img_src[w_aligned * 4], w * 4, w % 4, 4, block);
                EmitDXT5Block_SSE2<Is_YCoCg>(block, p_out);
            }
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<4 /* Channels */>(&img_src[i * 4], w * 4, _MIN(4, w - i), h % 4, block);
            EmitDXT5Block_SSE2<Is_YCoCg>(block, p_out);
        }
    } else
#endif
    {
        for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * 4) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_Ref<4 /* Channels */>(&img_src[i * 4], w * 4, block);
                EmitDXT5Block_Ref<Is_YCoCg>(block, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<4 /* Channels */>(&img_src[w_aligned * 4], w * 4, w % 4, 4, block);
                EmitDXT5Block_Ref<Is_YCoCg>(block, p_out);
            }
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<4 /* Channels */>(&img_src[i * 4], w * 4, _MIN(4, w - i), h % 4, block);
            EmitDXT5Block_Ref<Is_YCoCg>(block, p_out);
        }
    }
}

template void Ren::CompressImage_DXT5<false /* Is_YCoCg */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[]);
template void Ren::CompressImage_DXT5<true /* Is_YCoCg */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[]);

#undef _MIN
#undef _MAX

#undef force_inline
