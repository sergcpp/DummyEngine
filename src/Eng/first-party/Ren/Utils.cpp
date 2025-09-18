#include "Utils.h"

#include <array>
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
    eTexFormat::Undefined,   // DXGI_FORMAT_UNKNOWN
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32B32A32_TYPELESS
    eTexFormat::RGBA32F,     // DXGI_FORMAT_R32G32B32A32_FLOAT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32B32A32_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32B32A32_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32B32_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32B32_FLOAT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32B32_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32B32_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16B16A16_TYPELESS
    eTexFormat::RGBA16F,     // DXGI_FORMAT_R16G16B16A16_FLOAT
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16B16A16_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16B16A16_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16B16A16_SNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16B16A16_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32_TYPELESS
    eTexFormat::RG32F,       // DXGI_FORMAT_R32G32_FLOAT
    eTexFormat::RG32UI,      // DXGI_FORMAT_R32G32_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G32_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32G8X24_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_D32_FLOAT_S8X24_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_X32_TYPELESS_G8X24_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R10G10B10A2_TYPELESS
    eTexFormat::RGB10_A2,    // DXGI_FORMAT_R10G10B10A2_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R10G10B10A2_UINT
    eTexFormat::RG11F_B10F,  // DXGI_FORMAT_R11G11B10_FLOAT
    eTexFormat::Undefined,   // DXGI_FORMAT_R8G8B8A8_TYPELESS
    eTexFormat::RGBA8,       // DXGI_FORMAT_R8G8B8A8_UNORM
    eTexFormat::RGBA8,       // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    eTexFormat::Undefined,   // DXGI_FORMAT_R8G8B8A8_UINT
    eTexFormat::RGBA8_snorm, // DXGI_FORMAT_R8G8B8A8_SNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R8G8B8A8_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16_TYPELESS
    eTexFormat::RG16F,       // DXGI_FORMAT_R16G16_FLOAT
    eTexFormat::RG16,        // DXGI_FORMAT_R16G16_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16_SNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R16G16_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_D32_FLOAT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32_FLOAT
    eTexFormat::R32UI,       // DXGI_FORMAT_R32_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R32_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R24G8_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_D24_UNORM_S8_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_X24_TYPELESS_G8_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R8G8_TYPELESS
    eTexFormat::RG8,         // DXGI_FORMAT_R8G8_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R8G8_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R8G8_SNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R8G8_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R16_TYPELESS
    eTexFormat::R16F,        // DXGI_FORMAT_R16_FLOAT
    eTexFormat::Undefined,   // DXGI_FORMAT_D16_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R16_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R16_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R16_SNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R16_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R8_TYPELESS
    eTexFormat::R8,          // DXGI_FORMAT_R8_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R8_UINT
    eTexFormat::Undefined,   // DXGI_FORMAT_R8_SNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R8_SINT
    eTexFormat::Undefined,   // DXGI_FORMAT_A8_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R1_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R9G9B9E5_SHAREDEXP
    eTexFormat::Undefined,   // DXGI_FORMAT_R8G8_B8G8_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_G8R8_G8B8_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_BC1_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_BC1_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_BC1_UNORM_SRGB
    eTexFormat::Undefined,   // DXGI_FORMAT_BC2_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_BC2_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_BC2_UNORM_SRGB
    eTexFormat::Undefined,   // DXGI_FORMAT_BC3_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_BC3_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_BC3_UNORM_SRGB
    eTexFormat::Undefined,   // DXGI_FORMAT_BC4_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_BC4_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_BC4_SNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_BC5_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_BC5_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_BC5_SNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_B5G6R5_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_B5G5R5A1_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_B8G8R8A8_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_B8G8R8X8_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_B8G8R8A8_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
    eTexFormat::Undefined,   // DXGI_FORMAT_B8G8R8X8_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
    eTexFormat::Undefined,   // DXGI_FORMAT_BC6H_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_BC6H_UF16
    eTexFormat::Undefined,   // DXGI_FORMAT_BC6H_SF16
    eTexFormat::Undefined,   // DXGI_FORMAT_BC7_TYPELESS
    eTexFormat::Undefined,   // DXGI_FORMAT_BC7_UNORM
    eTexFormat::Undefined,   // DXGI_FORMAT_BC7_UNORM_SRGB
    eTexFormat::Undefined,   // DXGI_FORMAT_AYUV
    eTexFormat::Undefined,   // DXGI_FORMAT_Y410
    eTexFormat::Undefined,   // DXGI_FORMAT_Y416
    eTexFormat::Undefined,   // DXGI_FORMAT_NV12
    eTexFormat::Undefined,   // DXGI_FORMAT_P010
    eTexFormat::Undefined,   // DXGI_FORMAT_P016
    eTexFormat::Undefined,   // DXGI_FORMAT_420_OPAQUE
    eTexFormat::Undefined,   // DXGI_FORMAT_YUY2
    eTexFormat::Undefined,   // DXGI_FORMAT_Y210
    eTexFormat::Undefined,   // DXGI_FORMAT_Y216
    eTexFormat::Undefined,   // DXGI_FORMAT_NV11
    eTexFormat::Undefined,   // DXGI_FORMAT_AI44
    eTexFormat::Undefined,   // DXGI_FORMAT_IA44
    eTexFormat::Undefined,   // DXGI_FORMAT_P8
    eTexFormat::Undefined,   // DXGI_FORMAT_A8P8
    eTexFormat::Undefined,   // DXGI_FORMAT_B4G4R4A4_UNORM = 115
};
static_assert(std::size(g_tex_format_from_dxgi_format) == 116);

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

force_inline Vec4f permute(const Vec4f &x) { return Mod(((x * 34.0f) + Vec4f{1.0f}) * x, Vec4f{289.0f}); }

force_inline Vec4f taylor_inv_sqrt(const Vec4f &r) { return Vec4f{1.79284291400159f} - 0.85373472095314f * r; }

force_inline Vec3f fade(const Vec3f &t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
force_inline Vec4f fade(const Vec4f &t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

int round_up(int v, int align) { return align * ((v + align - 1) / align); }

// Skew constants for simplex noise
static const float F3 = 1.0f / 3.0f;
static const float G3 = 1.0f / 6.0f;

// Randomly shuffled numbers from 0 to 255
static const int g_perm[] = {
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240,
    21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88,
    237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83,
    111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80,
    73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64,
    52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182,
    189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22,
    39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210,
    144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84,
    204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78,
    66, 215, 61, 156, 180,
    // duplicate
    151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240,
    21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88,
    237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83,
    111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80,
    73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64,
    52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182,
    189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22,
    39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210,
    144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84,
    204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78,
    66, 215, 61, 156, 180};

// Gradients table (12 vectors + 4 repeated to use with 4bit index)
static const Vec3f g_grad3[] = {Vec3f{1, 1, 0}, Vec3f{-1, 1, 0}, Vec3f{1, -1, 0}, Vec3f{-1, -1, 0}, Vec3f{1, 0, 1},
                                Vec3f{-1, 0, 1}, Vec3f{1, 0, -1}, Vec3f{-1, 0, -1}, Vec3f{0, 1, 1}, Vec3f{0, -1, 1},
                                Vec3f{0, 1, -1}, Vec3f{0, -1, -1},
                                // redundant (round up to 16)
                                Vec3f{1, 1, 0}, Vec3f{0, -1, 1}, Vec3f{-1, 1, 0}, Vec3f{0, -1, -1}};

// Maps hash value to one of the 12 gradients and returns dot product with (x, y, z)
float grad(int hash, float x, float y, float z) {
    const int h = hash & 15;
    const float u = h < 8 ? x : y;
    const float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

Vec3f SinRandom3(const Vec3f &c) {
    float j = 4096.0f * std::sin(Dot(c, Vec3f{17.0f, 59.4f, 15.0f}));
    Vec3f r;
    float _unused;
    r[2] = std::modf(512.0f * j, &_unused);
    j *= 0.125f;
    r[0] = std::modf(512.0f * j, &_unused);
    j *= 0.125f;
    r[1] = std::modf(512.0f * j, &_unused);
    return r - 0.5f;
}

// https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
static const int RGB9E5_EXPONENT_BITS = 5;
static const int RGB9E5_MANTISSA_BITS = 9;
static const int RGB9E5_EXP_BIAS = 15;
static const int RGB9E5_MAX_VALID_BIASED_EXP = 31;

static const int MAX_RGB9E5_EXP = (RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS);
static const int RGB9E5_MANTISSA_VALUES = (1 << RGB9E5_MANTISSA_BITS);
static const int MAX_RGB9E5_MANTISSA = (RGB9E5_MANTISSA_VALUES - 1);
static const float MAX_RGB9E5 = (((float)MAX_RGB9E5_MANTISSA) / RGB9E5_MANTISSA_VALUES * (1 << MAX_RGB9E5_EXP));
[[maybe_unused]] static const float EPSILON_RGB9E5 = ((1.0f / RGB9E5_MANTISSA_VALUES) / (1 << RGB9E5_EXP_BIAS));

struct BitsOfIEEE754 {
    unsigned int mantissa : 23;
    unsigned int biasedexponent : 8;
    unsigned int negative : 1;
};

union float754 {
    unsigned int raw;
    float value;
    BitsOfIEEE754 field;
};

struct BitsOfRGB9E5 {
    unsigned int r : RGB9E5_MANTISSA_BITS;
    unsigned int g : RGB9E5_MANTISSA_BITS;
    unsigned int b : RGB9E5_MANTISSA_BITS;
    unsigned int biasedexponent : RGB9E5_EXPONENT_BITS;
};

union rgb9e5 {
    unsigned int raw;
    BitsOfRGB9E5 field;
};

float ClampRange_for_rgb9e5(float x) {
    if (x > 0) {
        if (x >= MAX_RGB9E5) {
            return MAX_RGB9E5;
        } else {
            return x;
        }
    } else {
        // NaN gets here too since comparisons with NaN always fail!
        return 0;
    }
}

int FloorLog2(float x) {
    float754 f = {};
    f.value = x;
    return (f.field.biasedexponent - 127);
}

rgb9e5 float3_to_rgb9e5(const float rgb[3]) {
    float rc = ClampRange_for_rgb9e5(rgb[0]);
    float gc = ClampRange_for_rgb9e5(rgb[1]);
    float bc = ClampRange_for_rgb9e5(rgb[2]);

    float maxrgb = _MAX3(rc, gc, bc);
    int exp_shared = _MAX(-RGB9E5_EXP_BIAS - 1, FloorLog2(maxrgb)) + 1 + RGB9E5_EXP_BIAS;
    assert(exp_shared <= RGB9E5_MAX_VALID_BIASED_EXP);
    assert(exp_shared >= 0);
    // This pow function could be replaced by a table.
    double denom = pow(2, exp_shared - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS);

    int maxm = int(floor(maxrgb / denom + 0.5));
    if (maxm == MAX_RGB9E5_MANTISSA + 1) {
        denom *= 2;
        exp_shared += 1;
        assert(exp_shared <= RGB9E5_MAX_VALID_BIASED_EXP);
    } else {
        assert(maxm <= MAX_RGB9E5_MANTISSA);
    }

    const int rm = int(floor(rc / denom + 0.5f));
    const int gm = int(floor(gc / denom + 0.5f));
    const int bm = int(floor(bc / denom + 0.5f));

    assert(rm <= MAX_RGB9E5_MANTISSA);
    assert(gm <= MAX_RGB9E5_MANTISSA);
    assert(bm <= MAX_RGB9E5_MANTISSA);
    assert(rm >= 0);
    assert(gm >= 0);
    assert(bm >= 0);

    rgb9e5 retval = {};
    retval.field.r = rm;
    retval.field.g = gm;
    retval.field.b = bm;
    retval.field.biasedexponent = exp_shared;

    return retval;
}

void rgb9e5_to_float3(rgb9e5 v, float retval[3]) {
    const int exponent = v.field.biasedexponent - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS;
    const auto scale = float(pow(2, exponent));

    retval[0] = v.field.r * scale;
    retval[1] = v.field.g * scale;
    retval[2] = v.field.b * scale;
}

} // namespace Ren

std::unique_ptr<uint8_t[]> Ren::ReadTGAFile(Span<const uint8_t> data, int &w, int &h, eTexFormat &format) {
    uint32_t img_size;
    ReadTGAFile(data, w, h, format, nullptr, img_size);

    std::unique_ptr<uint8_t[]> image_ret(new uint8_t[img_size]);
    ReadTGAFile(data, w, h, format, image_ret.get(), img_size);

    return image_ret;
}

bool Ren::ReadTGAFile(Span<const uint8_t> data, int &w, int &h, eTexFormat &format, uint8_t *out_data,
                      uint32_t &out_size) {
    const uint8_t tga_header[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t *tga_compare = data.data();
    const uint8_t *img_header = data.data() + sizeof(tga_header);
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
        format = eTexFormat::RGBA8;
    } else if (bpp == 24) {
        format = eTexFormat::RGB8;
    }

    if (out_data && out_size < w * h * bytes_per_pixel) {
        return false;
    }

    out_size = w * h * bytes_per_pixel;
    if (out_data) {
        const uint8_t *image_data = data.data() + 18;

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

std::vector<float> Ren::ConvertRGBE_to_RGB32F(Span<const uint8_t> image_data, const int w, const int h) {
    std::vector<float> fp_data(w * h * 3);

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

std::vector<uint8_t> Ren::ConvertRGB32F_to_RGBE(Span<const float> image_data, const int w, const int h,
                                                const int channels) {
    std::vector<uint8_t> u8_data(w * h * 4);

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
                if (exp[i] < -128) {
                    exp[i] = -128;
                } else if (exp[i] > 127) {
                    exp[i] = 127;
                }
            }

            const float common_exp = std::max(exp[0], std::max(exp[1], exp[2]));
            const float range = std::exp2(common_exp);

            Vec3f mantissa = val / range;
            for (int i = 0; i < 3; i++) {
                if (mantissa[i] < 0)
                    mantissa[i] = 0;
                else if (mantissa[i] > 1)
                    mantissa[i] = 1;
            }

            const auto res = Vec4f{mantissa[0], mantissa[1], mantissa[2], common_exp + 128};

            u8_data[(y * w + x) * 4 + 0] = (uint8_t)_CLAMP(int(res[0] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 1] = (uint8_t)_CLAMP(int(res[1] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 2] = (uint8_t)_CLAMP(int(res[2] * 255), 0, 255);
            u8_data[(y * w + x) * 4 + 3] = (uint8_t)_CLAMP(int(res[3]), 0, 255);
        }
    }

    return u8_data;
}

std::vector<uint8_t> Ren::ConvertRGB32F_to_RGBM(Span<const float> image_data, const int w, const int h,
                                                const int channels) {
    std::vector<uint8_t> u8_data(w * h * 4);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            RGBMEncode(&image_data[channels * (y * w + x)], &u8_data[(y * w + x) * 4]);
        }
    }

    return u8_data;
}

std::vector<uint32_t> Ren::ConvertRGB32F_to_RGB9E5(Span<const float> image_data, const int w, const int h) {
    std::vector<uint32_t> ret(w * h);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const float rgb[3] = {image_data[3 * (y * w + x) + 0], image_data[3 * (y * w + x) + 1],
                                  image_data[3 * (y * w + x) + 2]};
            ret[y * w + x] = float3_to_rgb9e5(rgb).raw;
        }
    }

    return ret;
}

std::vector<float> Ren::ConvertRGB9E5_to_RGB32F(Span<const uint32_t> image_data, int w, int h) {
    std::vector<float> ret(3 * w * h);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            rgb9e5 v;
            v.raw = image_data[y * w + x];
            rgb9e5_to_float3(v, &ret[3 * (y * w + x)]);
        }
    }

    return ret;
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
                     const eMipOp op[4], const int min_tex_dim) {
    int mip_count = 1;

    int _w = widths[0], _h = heights[0];
    while (_w > min_tex_dim && _h > min_tex_dim) {
        int _prev_w = _w, _prev_h = _h;
        _w = std::max(_w / 2, 1);
        _h = std::max(_h / 2, 1);
        if (!mipmaps[mip_count]) {
            mipmaps[mip_count] = std::make_unique<uint8_t[]>(_w * _h * channels);
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

int Ren::InitMipMapsRGBM(std::unique_ptr<uint8_t[]> mipmaps[16], int widths[16], int heights[16],
                         const int min_tex_dim) {
    int mip_count = 1;

    int _w = widths[0], _h = heights[0];
    while (_w > min_tex_dim && _h > min_tex_dim) {
        int _prev_w = _w, _prev_h = _h;
        _w = std::max(_w / 2, 1);
        _h = std::max(_h / 2, 1);
        mipmaps[mip_count] = std::make_unique<uint8_t[]>(_w * _h * 4);
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
        float score = 0;
        uint32_t ref_count = 0;
        uint32_t active_tris_count = 0;
        std::unique_ptr<int32_t[]> tris;
    };

    static const int MaxSizeVertexCache = 32;

    auto get_vertex_score = [](int32_t cache_pos, uint32_t active_tris_count) -> float {
        const float CacheDecayPower = 1.5f;
        const float LastTriScore = 0.75f;
        const float ValenceBoostScale = 2.0f;
        const float ValenceBoostPower = 0.5f;

        if (active_tris_count == 0) {
            // No tri needs this vertex!
            return -1.0f;
        }

        float score = 0;

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
        float score = 0;
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
        v.tris = std::make_unique<int32_t[]>(v.active_tris_count);
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
            next_next_best_index = i / 3u;
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

void Ren::ComputeTangentBasis(std::vector<vertex_t> &vertices, std::vector<uint32_t> index_groups[],
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

            const float det = fabsf(dt1[0] * dt2[1] - dt1[1] * dt2[0]);
            if (det > flt_eps) {
                const float inv_det = 1.0f / det;
                tangent = (dp1 * dt2[1] - dp2 * dt1[1]) * inv_det;
                binormal = (dp2 * dt1[0] - dp1 * dt2[0]) * inv_det;
            } else {
                const Vec3f plane_N = Cross(dp1, dp2);

                int w = 2;
                tangent = Vec3f{0, 1, 0};
                if (fabsf(plane_N[0]) <= fabsf(plane_N[1]) && fabsf(plane_N[0]) <= fabsf(plane_N[2])) {
                    tangent = Vec3f{1, 0, 0};
                    w = 1;
                } else if (fabsf(plane_N[2]) <= fabsf(plane_N[0]) && fabsf(plane_N[2]) <= fabsf(plane_N[1])) {
                    tangent = Vec3f{0, 0, 1};
                    w = 0;
                }

                if (fabsf(plane_N[w]) > flt_eps) {
                    binormal = Normalize(Cross(Vec3f(plane_N), tangent));
                    tangent = Normalize(Cross(Vec3f(plane_N), binormal));
                } else {
                    binormal = {};
                    tangent = {};
                }
            }

            int i1 = (v0->b[0] * tangent[0] + v0->b[1] * tangent[1] + v0->b[2] * tangent[2]) < 0;
            int i2 = 2 * (b0[0] * binormal[0] + b0[1] * binormal[1] + b0[2] * binormal[2] < 0);

            if (i1 || i2) {
                uint32_t index = twin_verts[indices[i + 0]][i1 + i2 - 1];
                if (index == 0) {
                    index = uint32_t(vertices.size());
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
                    index = uint32_t(vertices.size());
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
                    index = uint32_t(vertices.size());
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
        if (fabsf(v.b[0]) > flt_eps || fabsf(v.b[1]) > flt_eps || fabsf(v.b[2]) > flt_eps) {
            const Vec3f tangent = MakeVec3(v.b);
            Vec3f binormal = Cross(MakeVec3(v.n), tangent);
            const float l = Length(binormal);
            if (l > flt_eps) {
                binormal /= l;
                memcpy(&v.b[0], ValuePtr(binormal), 3 * sizeof(float));
            }
        }

        if (fabsf(v.b[0]) < flt_eps && fabsf(v.b[1]) < flt_eps && fabsf(v.b[2]) < flt_eps) {
            // Fallback to simple basis
            Vec3f tangent;
            if (fabsf(v.n[2]) > 0.0f) {
                float k = sqrtf(v.n[1] * v.n[1] + v.n[2] * v.n[2]);
                tangent[0] = 0.0f;
                tangent[1] = -v.n[2] / k;
                tangent[2] = v.n[1] / k;
            } else {
                float k = sqrtf(v.n[0] * v.n[0] + v.n[1] * v.n[1]);
                tangent[0] = v.n[1] / k;
                tangent[1] = -v.n[0] / k;
                tangent[2] = 0.0f;
            }
            Vec3f binormal = Cross(MakeVec3(v.n), tangent);
            const float l = Length(binormal);
            if (l > flt_eps) {
                binormal /= l;
                memcpy(&v.b[0], ValuePtr(binormal), 3 * sizeof(float));
            }
        }
    }
}

float Ren::PerlinNoise4D(const Vec4f &P) {
    Vec4f Pi0 = Floor(P);       // Integer part for indexing
    Vec4f Pi1 = Pi0 + Vec4f{1}; // Integer part + 1
    Pi0 = Mod(Pi0, Vec4f{289});
    Pi1 = Mod(Pi1, Vec4f{289});
    const Vec4f Pf0 = Fract(P);       // Fractional part for interpolation
    const Vec4f Pf1 = Pf0 - Vec4f{1}; // Fractional part - 1.0
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
    Vec4f gy00 = Floor(gx00) / 7.0f;
    Vec4f gz00 = Floor(gy00) / 6.0f;
    gx00 = Fract(gx00) - Vec4f{0.5f};
    gy00 = Fract(gy00) - Vec4f{0.5f};
    gz00 = Fract(gz00) - Vec4f{0.5f};
    Vec4f gw00 = Vec4f{0.75} - Abs(gx00) - Abs(gy00) - Abs(gz00);
    Vec4f sw00 = Step(gw00, Vec4f{0});
    gx00 -= sw00 * (Step(Vec4f{0}, gx00) - Vec4f{0.5f});
    gy00 -= sw00 * (Step(Vec4f{0}, gy00) - Vec4f{0.5f});

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
    gx10 = Fract(gx10) - Vec4f{0.5};
    gy10 = Fract(gy10) - Vec4f{0.5};
    gz10 = Fract(gz10) - Vec4f{0.5};
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
float Ren::PerlinNoise4D(const Vec4f &P, const Vec4f &rep) {
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

float Ren::PerlinNoise3D_Perm(const Vec3f &P) {
    const Vec3f grid_cell = Floor(P);
    const Vec3i grid_cell_i = Vec3i(grid_cell) % 256;
    const Vec3i grid_cell_n = (grid_cell_i + 1) % 256;

    const Vec3f rel_P = P - grid_cell;

    // Gradient indices
    const int gi000 = g_perm[grid_cell_i[0] + g_perm[grid_cell_i[1] + g_perm[grid_cell_i[2]]]] % 16;
    const int gi001 = g_perm[grid_cell_i[0] + g_perm[grid_cell_i[1] + g_perm[grid_cell_n[2]]]] % 16;
    const int gi010 = g_perm[grid_cell_i[0] + g_perm[grid_cell_n[1] + g_perm[grid_cell_i[2]]]] % 16;
    const int gi011 = g_perm[grid_cell_i[0] + g_perm[grid_cell_n[1] + g_perm[grid_cell_n[2]]]] % 16;
    const int gi100 = g_perm[grid_cell_n[0] + g_perm[grid_cell_i[1] + g_perm[grid_cell_i[2]]]] % 16;
    const int gi101 = g_perm[grid_cell_n[0] + g_perm[grid_cell_i[1] + g_perm[grid_cell_n[2]]]] % 16;
    const int gi110 = g_perm[grid_cell_n[0] + g_perm[grid_cell_n[1] + g_perm[grid_cell_i[2]]]] % 16;
    const int gi111 = g_perm[grid_cell_n[0] + g_perm[grid_cell_n[1] + g_perm[grid_cell_n[2]]]] % 16;

    // Calculate noise contributions from each of the eight corners
    const float n000 = Dot(g_grad3[gi000], rel_P);
    const float n100 = Dot(g_grad3[gi100], rel_P - Vec3f{1, 0, 0});
    const float n010 = Dot(g_grad3[gi010], rel_P - Vec3f{0, 1, 0});
    const float n110 = Dot(g_grad3[gi110], rel_P - Vec3f{1, 1, 0});
    const float n001 = Dot(g_grad3[gi001], rel_P - Vec3f{0, 0, 1});
    const float n101 = Dot(g_grad3[gi101], rel_P - Vec3f{1, 0, 1});
    const float n011 = Dot(g_grad3[gi011], rel_P - Vec3f{0, 1, 1});
    const float n111 = Dot(g_grad3[gi111], rel_P - Vec3f{1, 1, 1});

    // Compute the fade curve value
    const Vec3f uvw = fade(rel_P);

    // Interpolate along x the contributions from each of the corners
    const float nx00 = Mix(n000, n100, uvw[0]);
    const float nx01 = Mix(n001, n101, uvw[0]);
    const float nx10 = Mix(n010, n110, uvw[0]);
    const float nx11 = Mix(n011, n111, uvw[0]);

    // Interpolate the four results along y
    const float nxy0 = Mix(nx00, nx10, uvw[1]);
    const float nxy1 = Mix(nx01, nx11, uvw[1]);

    // Interpolate the two last results along z
    const float nxyz = Mix(nxy0, nxy1, uvw[2]);

    return nxyz;
}

float Ren::SimplexNoise3D_Perm(const Vec3f &P) {
    const float s = (P[0] + P[1] + P[2]) * F3;
    const Vec3i ijk = Vec3i(Floor(P + s));

    const float t = (ijk[0] + ijk[1] + ijk[2]) * G3;
    const Vec3f _XYZ0 = Vec3f(ijk) - t;
    const Vec3f xyz0 = P - _XYZ0;

    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
    Vec3i i1; // Offsets for second corner of simplex in (i,j,k) coords
    Vec3i i2; // Offsets for third corner of simplex in (i,j,k) coords
    if (xyz0[0] >= xyz0[1]) {
        if (xyz0[1] >= xyz0[2]) {
            // X Y Z order
            i1 = Vec3i{1, 0, 0};
            i2 = Vec3i{1, 1, 0};
        } else if (xyz0[0] >= xyz0[2]) {
            // X Z Y order
            i1 = Vec3i{1, 0, 0};
            i2 = Vec3i{1, 0, 1};
        } else {
            // Z X Y order
            i1 = Vec3i{0, 0, 1};
            i2 = Vec3i{1, 0, 1};
        }
    } else {
        if (xyz0[1] < xyz0[2]) {
            // Z Y X order
            i1 = Vec3i{0, 0, 1};
            i2 = Vec3i{0, 1, 1};
        } else if (xyz0[0] < xyz0[2]) {
            // Y Z X order
            i1 = Vec3i{0, 1, 0};
            i2 = Vec3i{0, 1, 1};
        } else {
            // Y X Z order
            i1 = Vec3i{0, 1, 0};
            i2 = Vec3i{1, 1, 0};
        }
    }

    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.
    Vec3f xyz1 = xyz0 - Vec3f(i1) + G3;        // Offsets for second corner in (x,y,z) coords
    Vec3f xyz2 = xyz0 - Vec3f(i2) + 2.0f * G3; // Offsets for third corner in (x,y,z) coords
    Vec3f xyz3 = xyz0 - 1.0f + 3.0f * G3;      // Offsets for last corner in (x,y,z) coords

    Vec3i _ijk = ijk % 256;
    const int gi0 = g_perm[_ijk[0] + g_perm[_ijk[1] + g_perm[_ijk[2]]]] % 16;
    const int gi1 = g_perm[_ijk[0] + i1[0] + g_perm[_ijk[1] + i1[1] + g_perm[_ijk[2] + i1[2]]]] % 16;
    const int gi2 = g_perm[_ijk[0] + i2[0] + g_perm[_ijk[1] + i2[1] + g_perm[_ijk[2] + i2[2]]]] % 16;
    const int gi3 = g_perm[_ijk[0] + 1 + g_perm[_ijk[1] + 1 + g_perm[_ijk[2] + 1]]] % 16;

    // Calculate the contribution from the four corners
    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f, n3 = 0.0f;

    float t0 = 0.6f - Length2(xyz0); // x0 * x0 - y0 * y0 - z0 * z0;
    if (t0 > 0.0f) {
        t0 *= t0;
        n0 = t0 * t0 * Dot(g_grad3[gi0], xyz0);
    }
    float t1 = 0.6f - Length2(xyz1);
    if (t1 > 0.0f) {
        t1 *= t1;
        n1 = t1 * t1 * Dot(g_grad3[gi1], xyz1);
    }
    float t2 = 0.6f - Length2(xyz2);
    if (t2 > 0.0f) {
        t2 *= t2;
        n2 = t2 * t2 * Dot(g_grad3[gi2], xyz2);
    }
    float t3 = 0.6f - Length2(xyz3);
    if (t3 > 0.0f) {
        t3 *= t3;
        n3 = t3 * t3 * Dot(g_grad3[gi3], xyz3);
    }

    return 32.0f * (n0 + n1 + n2 + n3);
}

float Ren::SimplexNoise3D_Rand(const Vec3f &P) {
    // 1. find current tetrahedron T and it's four vertices
    // s, s+i1, s+i2, s+1.0 - absolute skewed (integer) coordinates of T vertices
    // x, x1, x2, x3 - unskewed coordinates of p relative to each of T vertices

    // calculate s and x
    const Vec3f s = Floor(P + (P[0] + P[1] + P[2]) * F3);
    const Vec3f x = P - s + (s[0] + s[1] + s[2]) * G3;

    // calculate i1 and i2
    Vec3f e = Step(Vec3f(0.0f), x - Vec3f(x[1], x[2], x[0]));
    e[2] = Min(e[2], 3.0f - e[0] - e[1] - e[2]);
    assert(e[0] != 1.0f || e[1] != 1.0f || e[2] != 1.0f);
    Vec3f i1 = e * (1.0f - Vec3f(e[2], e[0], e[1]));
    Vec3f i2 = 1.0f - Vec3f(e[2], e[0], e[1]) * (1.0f - e);

    // x1, x2, x3
    const Vec3f x1 = x - i1 + G3;
    const Vec3f x2 = x - i2 + 2.0f * G3;
    const Vec3f x3 = x - 1.0f + 3.0f * G3;

    // calculate surflet weights
    Vec4f w;
    w[0] = Length2(x);
    w[1] = Length2(x1);
    w[2] = Length2(x2);
    w[3] = Length2(x3);

    // w fades from 0.6 at the center of the surflet to 0.0 at the margin
    w = Max(0.6f - w, Vec4f(0.0f));

    // calculate surflet components
    Vec4f d;
    d[0] = Dot(SinRandom3(s), x);
    d[1] = Dot(SinRandom3(s + i1), x1);
    d[2] = Dot(SinRandom3(s + i2), x2);
    d[3] = Dot(SinRandom3(s + 1.0f), x3);

    // multiply d by w^4
    w *= w;
    w *= w;
    d *= w;

    // 3. return the sum of the four surflets
    return Dot(d, Vec4f(52.0f));
}

//
// https://software.intel.com/sites/default/files/23/1d/324337_324337.pdf
//

namespace Ren {
template <int SrcChannels, int DstChannels = 4>
void Extract4x4Block_Ref(const uint8_t src[], const int stride, uint8_t dst[16 * DstChannels]) {
    if (SrcChannels == 4 && DstChannels == 4) {
        for (int j = 0; j < 4; j++) {
            memcpy(&dst[j * 4 * 4], src, 4 * 4);
            src += stride;
        }
    } else {
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                memcpy(&dst[i * DstChannels], &src[i * SrcChannels],
                       SrcChannels < DstChannels ? SrcChannels : DstChannels);
            }
            dst += 4 * DstChannels;
            src += stride;
        }
    }
}

template <int SrcChannels, int DstChannels = 4>
void ExtractIncomplete4x4Block_Ref(const uint8_t src[], const int stride, const int blck_w, const int blck_h,
                                   uint8_t dst[16 * DstChannels]) {
    if (SrcChannels == 4 && DstChannels == 4) {
        for (int j = 0; j < blck_h; j++) {
            assert(blck_w <= 4);
            memcpy(&dst[0], src, 4 * blck_w);
            for (int i = blck_w; i < 4; i++) {
                memcpy(&dst[i * 4], &dst[(blck_w - 1) * 4], 4);
            }
            dst += 4 * 4;
            src += stride;
        }
    } else {
        for (int j = 0; j < blck_h; j++) {
            for (int i = 0; i < blck_w; i++) {
                memcpy(&dst[i * DstChannels], &src[i * SrcChannels],
                       SrcChannels < DstChannels ? SrcChannels : DstChannels);
            }
            for (int i = blck_w; i < 4; i++) {
                memcpy(&dst[i * DstChannels], &dst[(blck_w - 1) * DstChannels], DstChannels);
            }
            dst += 4 * DstChannels;
            src += stride;
        }
    }
    uint8_t *dst2 = dst - 4 * DstChannels;
    for (int j = blck_h; j < 4; j++) {
        memcpy(dst, dst2, 4 * DstChannels);
        dst += 4 * DstChannels;
    }
}

// WARNING: Reads 4 bytes outside of block!
template <int Channels> void Extract4x4Block_SSSE3(const uint8_t src[], int stride, uint8_t dst[64]);
template <int Channels> void Extract4x4Block_NEON(const uint8_t src[], int stride, uint8_t dst[64]);

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

void GetMinMaxAlphaByBBox_Ref(const uint8_t block[16], uint8_t &min_alpha, uint8_t &max_alpha) {
    min_alpha = 255;
    max_alpha = 0;

    // clang-format off
    for (int i = 0; i < 16; i++) {
        if (block[i] < min_alpha) min_alpha = block[i];
        if (block[i] > max_alpha) max_alpha = block[i];
    }
    // clang-format on
}

template <bool UseAlpha = false, bool Is_YCoCg = false>
void GetMinMaxColorByBBox_SSE2(const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]);
template <bool UseAlpha = false, bool Is_YCoCg = false>
void GetMinMaxColorByBBox_NEON(const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]);

void GetMinMaxAlphaByBBox_SSE2(const uint8_t block[16], uint8_t &min_alpha, uint8_t &max_alpha);
void GetMinMaxAlphaByBBox_NEON(const uint8_t block[16], uint8_t &min_alpha, uint8_t &max_alpha);

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
void InsetYCoCgBBox_NEON(uint8_t min_color[4], uint8_t max_color[4]);

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
void SelectYCoCgDiagonal_NEON(const uint8_t block[64], uint8_t min_color[3], uint8_t max_color[3]);

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
void ScaleYCoCg_NEON(uint8_t block[64], uint8_t min_color[3], uint8_t max_color[3]);

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
void EmitColorIndices_NEON(const uint8_t block[64], const uint8_t min_color[4], const uint8_t max_color[4],
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

void EmitAlphaOnlyIndices_Ref(const uint8_t block[16], const uint8_t min_alpha, const uint8_t max_alpha,
                              uint8_t *&out_data) {
    uint8_t ind[16];

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
        const uint8_t a = block[i];

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
void EmitAlphaIndices_NEON(const uint8_t block[64], uint8_t min_alpha, uint8_t max_alpha, uint8_t *&out_data);

void EmitAlphaOnlyIndices_SSE2(const uint8_t block[16], uint8_t min_alpha, uint8_t max_alpha, uint8_t *&out_data);
void EmitAlphaOnlyIndices_NEON(const uint8_t block[16], uint8_t min_alpha, uint8_t max_alpha, uint8_t *&out_data);

void Emit_BC1_Block_Ref(const uint8_t block[64], uint8_t *&out_data) {
    uint8_t min_color[4], max_color[4];
    GetMinMaxColorByBBox_Ref(block, min_color, max_color);

    push_u16(rgb888_to_rgb565(max_color), out_data);
    push_u16(rgb888_to_rgb565(min_color), out_data);

    EmitColorIndices_Ref(block, min_color, max_color, out_data);
}

template <bool Is_YCoCg> void Emit_BC3_Block_Ref(uint8_t block[64], uint8_t *&out_data) {
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

void Emit_BC4_Block_Ref(uint8_t block[16], uint8_t *&out_data) {
    uint8_t min_alpha, max_alpha;
    GetMinMaxAlphaByBBox_Ref(block, min_alpha, max_alpha);

    //
    // Write alpha block
    //

    push_u8(max_alpha, out_data);
    push_u8(min_alpha, out_data);

    EmitAlphaOnlyIndices_Ref(block, min_alpha, max_alpha, out_data);
}

#if defined(__ARM_NEON__) || defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
void Emit_BC1_Block_NEON(const uint8_t block[64], uint8_t *&out_data) {
    alignas(16) uint8_t min_color[4], max_color[4];
    GetMinMaxColorByBBox_NEON(block, min_color, max_color);

    push_u16(rgb888_to_rgb565(max_color), out_data);
    push_u16(rgb888_to_rgb565(min_color), out_data);

    EmitColorIndices_NEON(block, min_color, max_color, out_data);
}

template <bool Is_YCoCg> void Emit_BC3_Block_NEON(uint8_t block[64], uint8_t *&out_data) {
    uint8_t min_color[4], max_color[4];
    GetMinMaxColorByBBox_NEON<true /* UseAlpha */, Is_YCoCg>(block, min_color, max_color);
    if (Is_YCoCg) {
        ScaleYCoCg_NEON(block, min_color, max_color);
        InsetYCoCgBBox_NEON(min_color, max_color);
        SelectYCoCgDiagonal_NEON(block, min_color, max_color);
    }

    //
    // Write alpha block
    //

    push_u8(max_color[3], out_data);
    push_u8(min_color[3], out_data);

    EmitAlphaIndices_NEON(block, min_color[3], max_color[3], out_data);

    //
    // Write color block
    //

    push_u16(rgb888_to_rgb565(max_color), out_data);
    push_u16(rgb888_to_rgb565(min_color), out_data);

    EmitColorIndices_NEON(block, min_color, max_color, out_data);
}

void Emit_BC4_Block_NEON(uint8_t block[16], uint8_t *&out_data) {
    uint8_t min_alpha, max_alpha;
    GetMinMaxAlphaByBBox_NEON(block, min_alpha, max_alpha);

    //
    // Write alpha block
    //

    push_u8(max_alpha, out_data);
    push_u8(min_alpha, out_data);

    EmitAlphaOnlyIndices_NEON(block, min_alpha, max_alpha, out_data);
}
#else
void Emit_BC1_Block_SSE2(const uint8_t block[64], uint8_t *&out_data) {
    alignas(16) uint8_t min_color[4], max_color[4];
    GetMinMaxColorByBBox_SSE2(block, min_color, max_color);

    push_u16(rgb888_to_rgb565(max_color), out_data);
    push_u16(rgb888_to_rgb565(min_color), out_data);

    EmitColorIndices_SSE2(block, min_color, max_color, out_data);
}

template <bool Is_YCoCg> void Emit_BC3_Block_SSE2(uint8_t block[64], uint8_t *&out_data) {
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

void Emit_BC4_Block_SSE2(uint8_t block[16], uint8_t *&out_data) {
    uint8_t min_alpha, max_alpha;
    GetMinMaxAlphaByBBox_SSE2(block, min_alpha, max_alpha);

    //
    // Write alpha block
    //

    push_u8(max_alpha, out_data);
    push_u8(min_alpha, out_data);

    EmitAlphaOnlyIndices_SSE2(block, min_alpha, max_alpha, out_data);
}
#endif

} // namespace Ren

int Ren::GetRequiredMemory_BC1(const int w, const int h, const int pitch_align) {
    return round_up(BlockSize_BC1 * ((w + 3) / 4), pitch_align) * ((h + 3) / 4);
}

int Ren::GetRequiredMemory_BC3(const int w, const int h, const int pitch_align) {
    return round_up(BlockSize_BC3 * ((w + 3) / 4), pitch_align) * ((h + 3) / 4);
}

int Ren::GetRequiredMemory_BC4(const int w, const int h, const int pitch_align) {
    return round_up(BlockSize_BC4 * ((w + 3) / 4), pitch_align) * ((h + 3) / 4);
}

int Ren::GetRequiredMemory_BC5(const int w, const int h, const int pitch_align) {
    return round_up(BlockSize_BC5 * ((w + 3) / 4), pitch_align) * ((h + 3) / 4);
}

template <int SrcChannels>
void Ren::CompressImage_BC1(const uint8_t img_src[], const int w, const int h, uint8_t img_dst[], int dst_pitch) {
    alignas(16) uint8_t block[64] = {};
    uint8_t *p_out = img_dst;

    const int w_aligned = w - (w % 4);
    const int h_aligned = h - (h % 4);

    const int pitch_pad = dst_pitch == 0 ? 0 : dst_pitch - BlockSize_BC1 * ((w + 3) / 4);

#if defined(__ARM_NEON__) || defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    for (int j = 0; j < h_aligned; j += 4, img_src += 4 * w * SrcChannels) {
        const int w_limited = (SrcChannels == 3 && j == h_aligned - 4 && h_aligned == h) ? w_aligned - 4 : w_aligned;
        for (int i = 0; i < w_limited; i += 4) {
            Extract4x4Block_Ref<SrcChannels>(&img_src[i * SrcChannels], w * SrcChannels, block);
            Emit_BC1_Block_NEON(block, p_out);
        }
        if (w_limited != w_aligned && w_aligned >= 4) {
            // process last block (avoid reading 4 bytes outside of range)
            Extract4x4Block_Ref<SrcChannels>(&img_src[(w_aligned - 4) * SrcChannels], w * SrcChannels, block);
            Emit_BC1_Block_NEON(block, p_out);
        }
        // process last (incomplete) column
        if (w_aligned != w) {
            ExtractIncomplete4x4Block_Ref<SrcChannels>(&img_src[w_aligned * SrcChannels], w * SrcChannels, w % 4, 4,
                                                       block);
            Emit_BC1_Block_NEON(block, p_out);
        }
        p_out += pitch_pad;
    }
    // process last (incomplete) row
    for (int i = 0; i < w && h_aligned != h; i += 4) {
        ExtractIncomplete4x4Block_Ref<SrcChannels>(&img_src[i * SrcChannels], w * SrcChannels, _MIN(4, w - i), h % 4,
                                                   block);
        Emit_BC1_Block_NEON(block, p_out);
    }
#else
    if (g_CpuFeatures.sse2_supported && g_CpuFeatures.ssse3_supported) {
        for (int j = 0; j < h_aligned; j += 4, img_src += 4 * w * SrcChannels) {
            const int w_limited =
                (SrcChannels == 3 && j == h_aligned - 4 && h_aligned == h) ? w_aligned - 4 : w_aligned;
            for (int i = 0; i < w_limited; i += 4) {
                Extract4x4Block_SSSE3<SrcChannels>(&img_src[i * SrcChannels], w * SrcChannels, block);
                Emit_BC1_Block_SSE2(block, p_out);
            }
            if (w_limited != w_aligned && w_aligned >= 4) {
                // process last block (avoid reading 4 bytes outside of range)
                Extract4x4Block_Ref<SrcChannels>(&img_src[(w_aligned - 4) * SrcChannels], w * SrcChannels, block);
                Emit_BC1_Block_SSE2(block, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<SrcChannels>(&img_src[w_aligned * SrcChannels], w * SrcChannels, w % 4, 4,
                                                           block);
                Emit_BC1_Block_SSE2(block, p_out);
            }
            p_out += pitch_pad;
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<SrcChannels>(&img_src[i * SrcChannels], w * SrcChannels, _MIN(4, w - i),
                                                       h % 4, block);
            Emit_BC1_Block_SSE2(block, p_out);
        }
    } else {
        for (int j = 0; j < h_aligned; j += 4, img_src += 4 * w * SrcChannels) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_Ref<SrcChannels>(&img_src[i * SrcChannels], w * SrcChannels, block);
                Emit_BC1_Block_Ref(block, p_out);
            }
            // process last column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<SrcChannels>(&img_src[w_aligned * SrcChannels], w * SrcChannels, w % 4, 4,
                                                           block);
                Emit_BC1_Block_Ref(block, p_out);
            }
            p_out += pitch_pad;
        }
        // process last row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<SrcChannels>(&img_src[i * SrcChannels], w * SrcChannels, _MIN(4, w - i),
                                                       h % 4, block);
            Emit_BC1_Block_Ref(block, p_out);
        }
    }
#endif
}

template void Ren::CompressImage_BC1<4 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);
template void Ren::CompressImage_BC1<3 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);

template <bool Is_YCoCg>
void Ren::CompressImage_BC3(const uint8_t img_src[], const int w, const int h, uint8_t img_dst[], int dst_pitch) {
    alignas(16) uint8_t block[64] = {};
    uint8_t *p_out = img_dst;

    const int w_aligned = w - (w % 4);
    const int h_aligned = h - (h % 4);

    const int pitch_pad = dst_pitch == 0 ? 0 : dst_pitch - BlockSize_BC3 * ((w + 3) / 4);

#if defined(__ARM_NEON__) || defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * 4) {
        for (int i = 0; i < w_aligned; i += 4) {
            Extract4x4Block_NEON<4>(&img_src[i * 4], w * 4, block);
            Emit_BC3_Block_NEON<Is_YCoCg>(block, p_out);
        }
        // process last (incomplete) column
        if (w_aligned != w) {
            ExtractIncomplete4x4Block_Ref<4>(&img_src[w_aligned * 4], w * 4, w % 4, 4, block);
            Emit_BC3_Block_NEON<Is_YCoCg>(block, p_out);
        }
        p_out += pitch_pad;
    }
    // process last (incomplete) row
    for (int i = 0; i < w && h_aligned != h; i += 4) {
        ExtractIncomplete4x4Block_Ref<4>(&img_src[i * 4], w * 4, _MIN(4, w - i), h % 4, block);
        Emit_BC3_Block_NEON<Is_YCoCg>(block, p_out);
    }
#else
    if (g_CpuFeatures.sse2_supported && g_CpuFeatures.ssse3_supported) {
        for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * 4) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_SSSE3<4 /* SrcChannels */>(&img_src[i * 4], w * 4, block);
                Emit_BC3_Block_SSE2<Is_YCoCg>(block, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<4 /* SrcChannels */>(&img_src[w_aligned * 4], w * 4, w % 4, 4, block);
                Emit_BC3_Block_SSE2<Is_YCoCg>(block, p_out);
            }
            p_out += pitch_pad;
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<4 /* SrcChannels */>(&img_src[i * 4], w * 4, _MIN(4, w - i), h % 4, block);
            Emit_BC3_Block_SSE2<Is_YCoCg>(block, p_out);
        }
    } else {
        for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * 4) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_Ref<4>(&img_src[i * 4], w * 4, block);
                Emit_BC3_Block_Ref<Is_YCoCg>(block, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<4>(&img_src[w_aligned * 4], w * 4, w % 4, 4, block);
                Emit_BC3_Block_Ref<Is_YCoCg>(block, p_out);
            }
            p_out += pitch_pad;
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<4>(&img_src[i * 4], w * 4, _MIN(4, w - i), h % 4, block);
            Emit_BC3_Block_Ref<Is_YCoCg>(block, p_out);
        }
    }
#endif
}

template void Ren::CompressImage_BC3<false /* Is_YCoCg */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                           int dst_pitch);
template void Ren::CompressImage_BC3<true /* Is_YCoCg */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);

template <int SrcChannels>
void Ren::CompressImage_BC4(const uint8_t img_src[], const int w, const int h, uint8_t img_dst[], int dst_pitch) {
    alignas(16) uint8_t block[16] = {};
    uint8_t *p_out = img_dst;

    const int w_aligned = w - (w % 4);
    const int h_aligned = h - (h % 4);

    const int pitch_pad = dst_pitch == 0 ? 0 : dst_pitch - BlockSize_BC4 * ((w + 3) / 4);

#if defined(__ARM_NEON__) || defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * SrcChannels) {
        for (int i = 0; i < w_aligned; i += 4) {
            Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels], w * SrcChannels, block);
            Emit_BC4_Block_NEON(block, p_out);
        }
        // process last (incomplete) column
        if (w_aligned != w) {
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels], w * SrcChannels, w % 4, 4,
                                                          block);
            Emit_BC4_Block_NEON(block, p_out);
        }
        p_out += pitch_pad;
    }
    // process last (incomplete) row
    for (int i = 0; i < w && h_aligned != h; i += 4) {
        ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels], w * SrcChannels, _MIN(4, w - i), h % 4,
                                                      block);
        Emit_BC4_Block_NEON(block, p_out);
    }
#else
    if (g_CpuFeatures.sse2_supported) {
        for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * SrcChannels) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels], w * SrcChannels, block);
                Emit_BC4_Block_SSE2(block, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels], w * SrcChannels, w % 4,
                                                              4, block);
                Emit_BC4_Block_SSE2(block, p_out);
            }
            p_out += pitch_pad;
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels], w * SrcChannels, _MIN(4, w - i),
                                                          h % 4, block);
            Emit_BC4_Block_SSE2(block, p_out);
        }
    } else {
        for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * SrcChannels) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels], w * SrcChannels, block);
                Emit_BC4_Block_Ref(block, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels], w * SrcChannels, w % 4,
                                                              4, block);
                Emit_BC4_Block_Ref(block, p_out);
            }
            p_out += pitch_pad;
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels], w * SrcChannels, _MIN(4, w - i),
                                                          h % 4, block);
            Emit_BC4_Block_Ref(block, p_out);
        }
    }
#endif
}

template void Ren::CompressImage_BC4<4 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);
template void Ren::CompressImage_BC4<3 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);
template void Ren::CompressImage_BC4<2 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);
template void Ren::CompressImage_BC4<1 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);

template <int SrcChannels>
void Ren::CompressImage_BC5(const uint8_t img_src[], const int w, const int h, uint8_t img_dst[], int dst_pitch) {
    alignas(16) uint8_t block1[16] = {}, block2[16] = {};
    uint8_t *p_out = img_dst;

    const int w_aligned = w - (w % 4);
    const int h_aligned = h - (h % 4);

    const int pitch_pad = dst_pitch == 0 ? 0 : dst_pitch - BlockSize_BC5 * ((w + 3) / 4);

#if defined(__ARM_NEON__) || defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * SrcChannels) {
        for (int i = 0; i < w_aligned; i += 4) {
            Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 0], w * SrcChannels, block1);
            Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 1], w * SrcChannels, block2);
            Emit_BC4_Block_NEON(block1, p_out);
            Emit_BC4_Block_NEON(block2, p_out);
        }
        // process last (incomplete) column
        if (w_aligned != w) {
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels + 0], w * SrcChannels, w % 4,
                                                          4, block1);
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels + 1], w * SrcChannels, w % 4,
                                                          4, block2);
            Emit_BC4_Block_NEON(block1, p_out);
            Emit_BC4_Block_NEON(block2, p_out);
        }
        p_out += pitch_pad;
    }
    // process last (incomplete) row
    for (int i = 0; i < w && h_aligned != h; i += 4) {
        ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 0], w * SrcChannels, _MIN(4, w - i),
                                                      h % 4, block1);
        ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 1], w * SrcChannels, _MIN(4, w - i),
                                                      h % 4, block2);
        Emit_BC4_Block_NEON(block1, p_out);
        Emit_BC4_Block_NEON(block2, p_out);
    }
#else
    if (g_CpuFeatures.sse2_supported) {
        for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * SrcChannels) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 0], w * SrcChannels, block1);
                Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 1], w * SrcChannels, block2);
                Emit_BC4_Block_SSE2(block1, p_out);
                Emit_BC4_Block_SSE2(block2, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels + 0], w * SrcChannels,
                                                              w % 4, 4, block1);
                ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels + 1], w * SrcChannels,
                                                              w % 4, 4, block2);
                Emit_BC4_Block_SSE2(block1, p_out);
                Emit_BC4_Block_SSE2(block2, p_out);
            }
            p_out += pitch_pad;
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 0], w * SrcChannels,
                                                          _MIN(4, w - i), h % 4, block1);
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 1], w * SrcChannels,
                                                          _MIN(4, w - i), h % 4, block2);
            Emit_BC4_Block_SSE2(block1, p_out);
            Emit_BC4_Block_SSE2(block2, p_out);
        }
    } else {
        for (int j = 0; j < h_aligned; j += 4, img_src += w * 4 * SrcChannels) {
            for (int i = 0; i < w_aligned; i += 4) {
                Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 0], w * SrcChannels, block1);
                Extract4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 1], w * SrcChannels, block2);
                Emit_BC4_Block_Ref(block1, p_out);
                Emit_BC4_Block_Ref(block2, p_out);
            }
            // process last (incomplete) column
            if (w_aligned != w) {
                ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels + 0], w * SrcChannels,
                                                              w % 4, 4, block1);
                ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[w_aligned * SrcChannels + 1], w * SrcChannels,
                                                              w % 4, 4, block2);
                Emit_BC4_Block_Ref(block1, p_out);
                Emit_BC4_Block_Ref(block2, p_out);
            }
            p_out += pitch_pad;
        }
        // process last (incomplete) row
        for (int i = 0; i < w && h_aligned != h; i += 4) {
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 0], w * SrcChannels,
                                                          _MIN(4, w - i), h % 4, block1);
            ExtractIncomplete4x4Block_Ref<SrcChannels, 1>(&img_src[i * SrcChannels + 1], w * SrcChannels,
                                                          _MIN(4, w - i), h % 4, block2);
            Emit_BC4_Block_Ref(block1, p_out);
            Emit_BC4_Block_Ref(block2, p_out);
        }
    }
#endif
}

template void Ren::CompressImage_BC5<4 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);
template void Ren::CompressImage_BC5<3 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);
template void Ren::CompressImage_BC5<2 /* SrcChannels */>(const uint8_t img_src[], int w, int h, uint8_t img_dst[],
                                                          int dst_pitch);

#undef _MIN
#undef _MAX

#undef force_inline
