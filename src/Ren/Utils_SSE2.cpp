#include "Utils.h"

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target("sse2")
#endif

#include <cassert>
#include <cstring>

#include <immintrin.h>
#include <xmmintrin.h>

#ifdef __linux__
#define _mm_storeu_si32(mem_addr, a) \
    _mm_store_ss((float*)mem_addr, _mm_castsi128_ps(a));
#define _mm_loadu_si32(mem_addr) \
    _mm_castps_si128(_mm_load_ss((float const*)mem_addr))
#endif

#include "CPUFeatures.h"

#define _ABS(x) ((x) < 0 ? -(x) : (x))

void Ren::CopyYChannel_16px(const uint8_t *y_src, const int y_stride, const int w,
                            const int h, uint8_t *y_dst) {
    assert(g_CpuFeatures.sse2_supported);
    auto *py_dst = reinterpret_cast<__m128i *>(y_dst);

    const bool is_input_aligned = uintptr_t(y_src) % 16 == 0 && y_stride % 16 == 0;
    const bool is_output_aligned = uintptr_t(py_dst) % 16 == 0;

    if (is_input_aligned && is_output_aligned) {
        if (g_CpuFeatures.sse41_supported) { // for stream load
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m128i *>(y_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i y_src_val =
                        _mm_stream_load_si128(const_cast<__m128i *>(py_img++));
                    _mm_stream_si128(py_dst++, y_src_val);
                }
                y_src += y_stride;
            }
        } else {
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m128i *>(y_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i y_src_val = _mm_load_si128(py_img++);
                    _mm_stream_si128(py_dst++, y_src_val);
                }
                y_src += y_stride;
            }
        }
    } else if (is_input_aligned) {
        if (g_CpuFeatures.sse41_supported) { // for stream load
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m128i *>(y_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i y_src_val =
                        _mm_stream_load_si128(const_cast<__m128i *>(py_img++));
                    _mm_storeu_si128(py_dst++, y_src_val);
                }
                y_src += y_stride;
            }
        } else {
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m128i *>(y_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i y_src_val = _mm_load_si128(py_img++);
                    _mm_storeu_si128(py_dst++, y_src_val);
                }
                y_src += y_stride;
            }
        }
    } else if (is_output_aligned) {
        for (int y = 0; y < h; ++y) {
            const auto *py_img = reinterpret_cast<const __m128i *>(y_src);
            for (int x = 0; x < w / 16; ++x) {
                const __m128i y_src_val = _mm_loadu_si128(py_img++);
                _mm_stream_si128(py_dst++, y_src_val);
            }
            y_src += y_stride;
        }
    } else {
        for (int y = 0; y < h; ++y) {
            const auto *py_img = reinterpret_cast<const __m128i *>(y_src);
            for (int x = 0; x < w / 16; ++x) {
                const __m128i y_src_val = _mm_loadu_si128(py_img++);
                _mm_storeu_si128(py_dst++, y_src_val);
            }
            y_src += y_stride;
        }
    }
}

void Ren::InterleaveUVChannels_16px(const uint8_t *u_src, const uint8_t *v_src,
                                    const int u_stride, const int v_stride, const int w,
                                    const int h, uint8_t *uv_dst) {
    auto *puv_dst = reinterpret_cast<__m128i *>(uv_dst);

    const bool is_input_aligned = uintptr_t(u_src) % 16 == 0 && u_stride % 16 == 0 &&
                                  uintptr_t(v_src) % 16 == 0 && v_stride % 16 == 0;
    const bool is_output_aligned = uintptr_t(puv_dst) % 16 == 0;

    if (is_input_aligned && is_output_aligned) {
        if (g_CpuFeatures.sse41_supported) { // for stream load
            for (int y = 0; y < h; ++y) {
                const auto *pu_img = reinterpret_cast<const __m128i *>(u_src);
                const auto *pv_img = reinterpret_cast<const __m128i *>(v_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i u_src_val =
                        _mm_stream_load_si128(const_cast<__m128i *>(pu_img++));
                    const __m128i v_src_val =
                        _mm_stream_load_si128(const_cast<__m128i *>(pv_img++));

                    const __m128i res0 = _mm_unpacklo_epi8(u_src_val, v_src_val);
                    const __m128i res1 = _mm_unpackhi_epi8(u_src_val, v_src_val);

                    _mm_stream_si128(puv_dst++, res0);
                    _mm_stream_si128(puv_dst++, res1);
                }
                u_src += u_stride;
                v_src += v_stride;
            }
        } else {
            for (int y = 0; y < h; ++y) {
                const auto *pu_img = reinterpret_cast<const __m128i *>(u_src);
                const auto *pv_img = reinterpret_cast<const __m128i *>(v_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i u_src_val = _mm_load_si128(pu_img++);
                    const __m128i v_src_val = _mm_load_si128(pv_img++);

                    const __m128i res0 = _mm_unpacklo_epi8(u_src_val, v_src_val);
                    const __m128i res1 = _mm_unpackhi_epi8(u_src_val, v_src_val);

                    _mm_stream_si128(puv_dst++, res0);
                    _mm_stream_si128(puv_dst++, res1);
                }
                u_src += u_stride;
                v_src += v_stride;
            }
        }
    } else if (is_input_aligned) {
        if (g_CpuFeatures.sse41_supported) { // for stream load
            for (int y = 0; y < h; ++y) {
                const auto *pu_img = reinterpret_cast<const __m128i *>(u_src);
                const auto *pv_img = reinterpret_cast<const __m128i *>(v_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i u_src_val =
                        _mm_stream_load_si128(const_cast<__m128i *>(pu_img++));
                    const __m128i v_src_val =
                        _mm_stream_load_si128(const_cast<__m128i *>(pv_img++));

                    const __m128i res0 = _mm_unpacklo_epi8(u_src_val, v_src_val);
                    const __m128i res1 = _mm_unpackhi_epi8(u_src_val, v_src_val);

                    _mm_storeu_si128(puv_dst++, res0);
                    _mm_storeu_si128(puv_dst++, res1);
                }
                u_src += u_stride;
                v_src += v_stride;
            }
        } else {
            for (int y = 0; y < h; ++y) {
                const auto *pu_img = reinterpret_cast<const __m128i *>(u_src);
                const auto *pv_img = reinterpret_cast<const __m128i *>(v_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i u_src_val = _mm_load_si128(pu_img++);
                    const __m128i v_src_val = _mm_load_si128(pv_img++);

                    const __m128i res0 = _mm_unpacklo_epi8(u_src_val, v_src_val);
                    const __m128i res1 = _mm_unpackhi_epi8(u_src_val, v_src_val);

                    _mm_storeu_si128(puv_dst++, res0);
                    _mm_storeu_si128(puv_dst++, res1);
                }
                u_src += u_stride;
                v_src += v_stride;
            }
        }
    } else if (is_output_aligned) {
        for (int y = 0; y < h; ++y) {
            const auto *pu_img = reinterpret_cast<const __m128i *>(u_src);
            const auto *pv_img = reinterpret_cast<const __m128i *>(v_src);
            for (int x = 0; x < w / 16; ++x) {
                const __m128i u_src_val = _mm_loadu_si128(pu_img++);
                const __m128i v_src_val = _mm_loadu_si128(pv_img++);

                const __m128i res0 = _mm_unpacklo_epi8(u_src_val, v_src_val);
                const __m128i res1 = _mm_unpackhi_epi8(u_src_val, v_src_val);

                _mm_stream_si128(puv_dst++, res0);
                _mm_stream_si128(puv_dst++, res1);
            }
            u_src += u_stride;
            v_src += v_stride;
        }
    } else {
        for (int y = 0; y < h; ++y) {
            const auto *pu_img = reinterpret_cast<const __m128i *>(u_src);
            const auto *pv_img = reinterpret_cast<const __m128i *>(v_src);
            for (int x = 0; x < w / 16; ++x) {
                const __m128i u_src_val = _mm_loadu_si128(pu_img++);
                const __m128i v_src_val = _mm_loadu_si128(pv_img++);

                const __m128i res0 = _mm_unpacklo_epi8(u_src_val, v_src_val);
                const __m128i res1 = _mm_unpackhi_epi8(u_src_val, v_src_val);

                _mm_storeu_si128(puv_dst++, res0);
                _mm_storeu_si128(puv_dst++, res1);
            }
            u_src += u_stride;
            v_src += v_stride;
        }
    }
}

//
// Internal functions
//

// https://developer.download.nvidia.com/whitepapers/2007/Real-Time-YCoCg-DXT-Compression/Real-Time%20YCoCg-DXT%20Compression.pdf

// clang-format off
// Copy rgb values and zero out alpha
static const __m128i RGB_to_RGBA = _mm_set_epi8(-1 /* Insert zero */, 11, 10, 9,
    -1 /* Insert zero */, 8, 7, 6,
    -1 /* Insert zero */, 5, 4, 3,
    -1 /* Insert zero */, 2, 1, 0);
// clang-format on

// WARNING: Reads 4 bytes outside of block in 3 channels case!
namespace Ren {
template <int Channels>
void Extract4x4Block_SSSE3(const uint8_t src[], const int stride, uint8_t dst[64]) {
    for (int j = 0; j < 4; j++) {
        __m128i rgba;
        if (Channels == 4) {
            rgba = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src));
        } else if (Channels == 3) {
            const __m128i rgb = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src));
            rgba = _mm_shuffle_epi8(rgb, RGB_to_RGBA);
        }

        _mm_store_si128(reinterpret_cast<__m128i *>(dst), rgba);

        src += stride;
        dst += 4 * 4;
    }
}

template void Extract4x4Block_SSSE3<4 /* Channels */>(const uint8_t src[],
                                                      const int stride, uint8_t dst[64]);
template void Extract4x4Block_SSSE3<3 /* Channels */>(const uint8_t src[],
                                                      const int stride, uint8_t dst[64]);

static const __m128i CoCgInsetMul = _mm_set_epi16(1, 2, 2, 2, 1, 2, 2, 2);

template <bool UseAlpha, bool Is_YCoCg>
void GetMinMaxColorByBBox_SSE2(const uint8_t block[64], uint8_t min_color[4],
                               uint8_t max_color[4]) {
    __m128i min_col = _mm_set1_epi8(-1 /* 255 */);
    __m128i max_col = _mm_set1_epi8(0);

    const auto *_4px_lines = reinterpret_cast<const __m128i *>(block);

    for (int i = 0; i < 4; i++) {
        min_col = _mm_min_epu8(min_col, _4px_lines[i]);
        max_col = _mm_max_epu8(max_col, _4px_lines[i]);
    }

    // Find horizontal min/max values
    min_col = _mm_min_epu8(min_col, _mm_srli_si128(min_col, 8));
    min_col = _mm_min_epu8(min_col, _mm_srli_si128(min_col, 4));

    max_col = _mm_max_epu8(max_col, _mm_srli_si128(max_col, 8));
    max_col = _mm_max_epu8(max_col, _mm_srli_si128(max_col, 4));

    if (!Is_YCoCg) {
        __m128i min_col_16 = _mm_unpacklo_epi8(min_col, _mm_setzero_si128());
        __m128i max_col_16 = _mm_unpacklo_epi8(max_col, _mm_setzero_si128());
        __m128i inset = _mm_sub_epi16(max_col_16, min_col_16);
        if (!UseAlpha) {
            inset = _mm_srli_epi16(inset, 4);
        } else {
            inset = _mm_mullo_epi16(inset, CoCgInsetMul);
            inset = _mm_srli_epi16(inset, 5);
        }
        min_col_16 = _mm_add_epi16(min_col_16, inset);
        max_col_16 = _mm_sub_epi16(max_col_16, inset);
        min_col = _mm_packus_epi16(min_col_16, min_col_16);
        max_col = _mm_packus_epi16(max_col_16, max_col_16);
    }

    _mm_storeu_si32(min_color, min_col);
    _mm_storeu_si32(max_color, max_col);
}

template void GetMinMaxColorByBBox_SSE2<false /* UseAlpha */, false /* Is_YCoCg */>(
    const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]);
template void GetMinMaxColorByBBox_SSE2<true /* UseAlpha */, false /* Is_YCoCg */>(
    const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]);
template void GetMinMaxColorByBBox_SSE2<true /* UseAlpha */, true /* Is_YCoCg */>(
    const uint8_t block[64], uint8_t min_color[4], uint8_t max_color[4]);

static const __m128i YCoCgScaleBias =
    _mm_set_epi8(0, 0, -128, -128, 0, 0, -128, -128, 0, 0, -128, -128, 0, 0, -128, -128);

void ScaleYCoCg_SSE2(uint8_t block[64], uint8_t min_color[3], uint8_t max_color[3]) {
    int m0 = _ABS(min_color[0] - 128);
    int m1 = _ABS(min_color[1] - 128);
    int m2 = _ABS(max_color[0] - 128);
    int m3 = _ABS(max_color[1] - 128);

    // clang-format off
    if (m1 > m0) m0 = m1;
    if (m3 > m2) m2 = m3;
    if (m2 > m0) m0 = m2;
    // clang-format on

    static const int s0 = 128 / 2 - 1;
    static const int s1 = 128 / 4 - 1;

    const int mask0 = -(m0 <= s0);
    const int mask1 = -(m0 <= s1);
    const int scale = 1 + (1 & mask0) + (2 & mask1);

    min_color[0] = (min_color[0] - 128) * scale + 128;
    min_color[1] = (min_color[1] - 128) * scale + 128;
    min_color[2] = (scale - 1) * 8;

    max_color[0] = (max_color[0] - 128) * scale + 128;
    max_color[1] = (max_color[1] - 128) * scale + 128;
    max_color[2] = (scale - 1) * 8;

    const __m128i _scale = _mm_set_epi16(1, scale, 1, scale, 1, scale, 1, scale);

    const __m128i _mask = _mm_set_epi8(-1, -1, ~(scale - 1), ~(scale - 1), -1, -1,
                                       ~(scale - 1), ~(scale - 1), -1, -1, ~(scale - 1),
                                       ~(scale - 1), -1, -1, ~(scale - 1), ~(scale - 1));

    auto *_4px_lines = reinterpret_cast<__m128i *>(block);

    for (int i = 0; i < 4; i++) {
        __m128i cur_col = _mm_load_si128(&_4px_lines[i]);

        cur_col = _mm_add_epi8(cur_col, YCoCgScaleBias);
        cur_col = _mm_mullo_epi16(cur_col, _scale);
        cur_col = _mm_and_si128(cur_col, _mask);
        cur_col = _mm_sub_epi8(cur_col, YCoCgScaleBias);

        _mm_store_si128(&_4px_lines[i], cur_col);
    }
}

static const int InsetColorShift = 4;
static const int InsetAlphaShift = 5;

static const __m128i InsetYCoCgRound = _mm_set_epi16(
    0, 0, 0, 0, (1 << (InsetAlphaShift - 1)) - 1, (1 << (InsetColorShift - 1)) - 1,
    (1 << (InsetColorShift - 1)) - 1, (1 << (InsetColorShift - 1)) - 1);
static const __m128i InsetYCoCgMask = _mm_set_epi16(-1, 0, -1, -1, -1, 0, -1, -1);

static const __m128i InsetShiftUp =
    _mm_set_epi16(0, 0, 0, 0, 1 << InsetAlphaShift, 1 << InsetColorShift,
                  1 << InsetColorShift, 1 << InsetColorShift);
static const __m128i InsetShiftDown =
    _mm_set_epi16(0, 0, 0, 0, 1 << (16 - InsetAlphaShift), 1 << (16 - InsetColorShift),
                  1 << (16 - InsetColorShift), 1 << (16 - InsetColorShift));

static const __m128i Inset565Mask = _mm_set_epi16(
    0xff, 0b11111000, 0b11111100, 0b11111000, 0xff, 0b11111000, 0b11111100, 0b11111000);
static const __m128i Inset565Rep =
    _mm_set_epi16(0, 1 << (16 - 5), 1 << (16 - 6), 1 << (16 - 5), 0, 1 << (16 - 5),
                  1 << (16 - 6), 1 << (16 - 5));

void InsetYCoCgBBox_SSE2(uint8_t min_color[4], uint8_t max_color[4]) {
    __m128i min_col, max_col;

    min_col = _mm_loadu_si32(min_color);
    max_col = _mm_loadu_si32(max_color);

    min_col = _mm_unpacklo_epi8(min_col, _mm_setzero_si128());
    max_col = _mm_unpacklo_epi8(max_col, _mm_setzero_si128());

    __m128i inset = _mm_sub_epi16(max_col, min_col);
    inset = _mm_sub_epi16(inset, InsetYCoCgRound);
    inset = _mm_and_si128(inset, InsetYCoCgMask);

    min_col = _mm_mullo_epi16(min_col, InsetShiftUp);
    max_col = _mm_mullo_epi16(max_col, InsetShiftUp);

    min_col = _mm_add_epi16(min_col, inset);
    max_col = _mm_sub_epi16(max_col, inset);

    min_col = _mm_mulhi_epi16(min_col, InsetShiftDown);
    max_col = _mm_mulhi_epi16(max_col, InsetShiftDown);

    min_col = _mm_max_epi16(min_col, _mm_setzero_si128());
    max_col = _mm_max_epi16(max_col, _mm_setzero_si128());

    min_col = _mm_and_si128(min_col, Inset565Mask);
    max_col = _mm_and_si128(max_col, Inset565Mask);

    const __m128i temp0 = _mm_mulhi_epi16(min_col, Inset565Rep);
    const __m128i temp1 = _mm_mulhi_epi16(max_col, Inset565Rep);

    min_col = _mm_or_si128(min_col, temp0);
    max_col = _mm_or_si128(max_col, temp1);

    min_col = _mm_packus_epi16(min_col, min_col);
    max_col = _mm_packus_epi16(max_col, max_col);

    _mm_storeu_si32(min_color, min_col);
    _mm_storeu_si32(max_color, max_col);
}

static const __m128i CoCgMask = _mm_set_epi16(0, -1, 0, -1, 0, -1, 0, -1);
static const __m128i CoCgDiagonalMask =
    _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0);

static const __m128i Zeroes_128 = _mm_setzero_si128();
static const __m128i Ones_i16 = _mm_set1_epi16(1);
static const __m128i Twos_i16 = _mm_set1_epi16(2);
static const __m128i Eights_i16 = _mm_set1_epi16(8);

void SelectYCoCgDiagonal_SSE2(const uint8_t block[64], uint8_t min_color[3],
                              uint8_t max_color[3]) {
    // load block
    __m128i line0 = _mm_load_si128(reinterpret_cast<const __m128i *>(block));
    __m128i line1 = _mm_load_si128(reinterpret_cast<const __m128i *>(block + 16));
    __m128i line2 = _mm_load_si128(reinterpret_cast<const __m128i *>(block + 32));
    __m128i line3 = _mm_load_si128(reinterpret_cast<const __m128i *>(block + 48));

    // mask out everything except CoCg channels
    __m128i line0_CoCg = _mm_and_si128(line0, CoCgMask);
    __m128i line1_CoCg = _mm_and_si128(line1, CoCgMask);
    __m128i line2_CoCg = _mm_and_si128(line2, CoCgMask);
    __m128i line3_CoCg = _mm_and_si128(line3, CoCgMask);

    // merge pairs of CoCg channels
    __m128i line01_CoCg = _mm_or_si128(line0_CoCg, _mm_slli_si128(line1_CoCg, 2));
    __m128i line23_CoCg = _mm_or_si128(line2_CoCg, _mm_slli_si128(line3_CoCg, 2));

    __m128i min_col = _mm_loadu_si32(min_color);
    __m128i max_col = _mm_loadu_si32(max_color);

    __m128i mid = _mm_avg_epu8(min_col, max_col);
    mid = _mm_shufflelo_epi16(mid, _MM_SHUFFLE(0, 0, 0, 0));
    mid = _mm_shuffle_epi32(mid, _MM_SHUFFLE(0, 0, 0, 0));

    __m128i tmp1 = _mm_max_epu8(mid, line01_CoCg);
    __m128i tmp3 = _mm_max_epu8(mid, line23_CoCg);
    tmp1 = _mm_cmpeq_epi8(tmp1, line01_CoCg);
    tmp3 = _mm_cmpeq_epi8(tmp3, line23_CoCg);

    __m128i tmp0 = _mm_srli_si128(tmp1, 1);
    __m128i tmp2 = _mm_srli_si128(tmp3, 1);

    tmp0 = _mm_xor_si128(tmp0, tmp1);
    tmp2 = _mm_xor_si128(tmp2, tmp3);
    tmp0 = _mm_and_si128(tmp0, Ones_i16);
    tmp2 = _mm_and_si128(tmp2, Ones_i16);

    tmp0 = _mm_add_epi16(tmp0, tmp2);
    tmp0 = _mm_sad_epu8(tmp0, Zeroes_128);
    tmp1 = _mm_shuffle_epi32(tmp0, _MM_SHUFFLE(1, 0, 3, 2));

    tmp0 = _mm_add_epi16(tmp0, tmp1);
    tmp0 = _mm_cmpgt_epi16(tmp0, Eights_i16);
    tmp0 = _mm_and_si128(tmp0, CoCgDiagonalMask);

    min_col = _mm_xor_si128(min_col, max_col);
    tmp0 = _mm_and_si128(tmp0, min_col);
    max_col = _mm_xor_si128(max_col, tmp0);
    min_col = _mm_xor_si128(min_col, max_col);

    _mm_storeu_si32(min_color, min_col);
    _mm_storeu_si32(max_color, max_col);
}

static const __m128i RGBMask =
    _mm_set_epi8(0, 0, 0, 0, 0, -1, -1, -1, 0, 0, 0, 0, 0, -1, -1, -1);
// multiplier used to emulate division by 3
static const __m128i DivBy3_i16 = _mm_set1_epi16((1 << 16) / 3 + 1);

void EmitColorIndices_SSE2(const uint8_t block[64], const uint8_t min_color[4],
                           const uint8_t max_color[4], uint8_t *&out_data) {
    __m128i result = _mm_setzero_si128();

    __m128i color0 = _mm_loadu_si32(max_color);
    color0 = _mm_and_si128(color0, RGBMask);
    color0 = _mm_shuffle_epi32(color0, _MM_SHUFFLE(1, 0, 1, 0));

    __m128i color1 = _mm_loadu_si32(min_color);
    color1 = _mm_and_si128(color1, RGBMask);
    color1 = _mm_shuffle_epi32(color1, _MM_SHUFFLE(1, 0, 1, 0));

    __m128i color2 = _mm_unpacklo_epi8(color0, _mm_setzero_si128());
    __m128i color3 = _mm_unpacklo_epi8(color1, _mm_setzero_si128());

    __m128i tmp = color3;
    color3 = _mm_add_epi16(color3, color2);
    color2 = _mm_add_epi16(color2, color3);
    color2 = _mm_mulhi_epi16(color2, DivBy3_i16);
    color2 = _mm_packus_epi16(color2, _mm_setzero_si128());
    color2 = _mm_shuffle_epi32(color2, _MM_SHUFFLE(1, 0, 1, 0));

    color3 = _mm_add_epi16(color3, tmp);
    color3 = _mm_mulhi_epi16(color3, DivBy3_i16);
    color3 = _mm_packus_epi16(color3, _mm_setzero_si128());
    color3 = _mm_shuffle_epi32(color3, _MM_SHUFFLE(1, 0, 1, 0));

    // disassembled from original source code :(
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    for (int i = 32; i >= 0; i -= 32) {
        xmm3 = _mm_loadu_si64(block + i + 0);
        xmm3 = _mm_shuffle_epi32(xmm3, _MM_SHUFFLE(3, 1, 2, 0));
        xmm5 = _mm_loadu_si64(block + i + 8);
        xmm5 = _mm_shuffle_epi32(xmm5, _MM_SHUFFLE(3, 1, 2, 0));

        xmm0 = xmm3;
        xmm6 = xmm5;
        xmm0 = _mm_sad_epu8(xmm0, color0);
        xmm6 = _mm_sad_epu8(xmm6, color0);
        xmm0 = _mm_packs_epi32(xmm0, xmm6);
        xmm1 = xmm3;
        xmm6 = xmm5;
        xmm1 = _mm_sad_epu8(xmm1, color1);
        xmm6 = _mm_sad_epu8(xmm6, color1);
        xmm1 = _mm_packs_epi32(xmm1, xmm6);
        xmm2 = xmm3;
        xmm6 = xmm5;
        xmm2 = _mm_sad_epu8(xmm2, color2);
        xmm6 = _mm_sad_epu8(xmm6, color2);
        xmm2 = _mm_packs_epi32(xmm2, xmm6);
        xmm3 = _mm_sad_epu8(xmm3, color3);
        xmm5 = _mm_sad_epu8(xmm5, color3);
        xmm3 = _mm_packs_epi32(xmm3, xmm5);

        xmm4 = _mm_loadu_si64(block + i + 16);
        xmm4 = _mm_shuffle_epi32(xmm4, _MM_SHUFFLE(3, 1, 2, 0));
        xmm5 = _mm_loadu_si64(block + i + 24);
        xmm5 = _mm_shuffle_epi32(xmm5, _MM_SHUFFLE(3, 1, 2, 0));

        xmm6 = xmm4;
        xmm7 = xmm5;
        xmm6 = _mm_sad_epu8(xmm6, color0);
        xmm7 = _mm_sad_epu8(xmm7, color0);
        xmm6 = _mm_packs_epi32(xmm6, xmm7);
        xmm0 = _mm_packs_epi32(xmm0, xmm6);
        xmm6 = xmm4;
        xmm7 = xmm5;
        xmm6 = _mm_sad_epu8(xmm6, color1);
        xmm7 = _mm_sad_epu8(xmm7, color1);
        xmm6 = _mm_packs_epi32(xmm6, xmm7);
        xmm1 = _mm_packs_epi32(xmm1, xmm6);
        xmm6 = xmm4;
        xmm7 = xmm5;
        xmm6 = _mm_sad_epu8(xmm6, color2);
        xmm7 = _mm_sad_epu8(xmm7, color2);
        xmm6 = _mm_packs_epi32(xmm6, xmm7);
        xmm2 = _mm_packs_epi32(xmm2, xmm6);
        xmm4 = _mm_sad_epu8(xmm4, color3);
        xmm5 = _mm_sad_epu8(xmm5, color3);
        xmm4 = _mm_packs_epi32(xmm4, xmm5);
        xmm3 = _mm_packs_epi32(xmm3, xmm4);

        xmm7 = result;
        xmm7 = _mm_slli_epi32(xmm7, 16);

        xmm4 = xmm0;
        xmm5 = xmm1;
        xmm0 = _mm_cmpgt_epi16(xmm0, xmm3);
        xmm1 = _mm_cmpgt_epi16(xmm1, xmm2);
        xmm4 = _mm_cmpgt_epi16(xmm4, xmm2);
        xmm5 = _mm_cmpgt_epi16(xmm5, xmm3);
        xmm2 = _mm_cmpgt_epi16(xmm2, xmm3);
        xmm4 = _mm_and_si128(xmm4, xmm1);
        xmm5 = _mm_and_si128(xmm5, xmm0);
        xmm2 = _mm_and_si128(xmm2, xmm0);
        xmm4 = _mm_or_si128(xmm4, xmm5);
        xmm2 = _mm_and_si128(xmm2, Ones_i16);
        xmm4 = _mm_and_si128(xmm4, Twos_i16);
        xmm2 = _mm_or_si128(xmm2, xmm4);

        xmm5 = _mm_shuffle_epi32(xmm2, _MM_SHUFFLE(1, 0, 3, 2));
        xmm2 = _mm_unpacklo_epi16(xmm2, Zeroes_128);
        xmm5 = _mm_unpacklo_epi16(xmm5, Zeroes_128);
        xmm5 = _mm_slli_epi32(xmm5, 8);
        xmm7 = _mm_or_si128(xmm7, xmm5);
        xmm7 = _mm_or_si128(xmm7, xmm2);
        result = xmm7;
    }

    xmm4 = _mm_shuffle_epi32(xmm7, _MM_SHUFFLE(0, 3, 2, 1));
    xmm5 = _mm_shuffle_epi32(xmm7, _MM_SHUFFLE(1, 0, 3, 2));
    xmm6 = _mm_shuffle_epi32(xmm7, _MM_SHUFFLE(2, 1, 0, 3));
    xmm4 = _mm_slli_epi32(xmm4, 2);
    xmm5 = _mm_slli_epi32(xmm5, 4);
    xmm6 = _mm_slli_epi32(xmm6, 6);
    xmm7 = _mm_or_si128(xmm7, xmm4);
    xmm7 = _mm_or_si128(xmm7, xmm5);
    xmm7 = _mm_or_si128(xmm7, xmm6);
    _mm_storeu_si32(out_data, xmm7);
    out_data += 4;
}

// multiplier used to emulate division by 7
static const __m128i DivBy7_i16 = _mm_set1_epi16((1 << 16) / 7 + 1);
// multiplier used to emulate division by 14
static const __m128i DivBy14_i16 = _mm_set1_epi16((1 << 16) / 14 + 1);
static const __m128i ScaleBy_66554400_i16 = _mm_setr_epi16(6, 6, 5, 5, 4, 4, 0, 0);
static const __m128i ScaleBy_11223300_i16 = _mm_setr_epi16(1, 1, 2, 2, 3, 3, 0, 0);

static const __m128i Ones_i8 = _mm_set1_epi8(1);
static const __m128i Twos_i8 = _mm_set1_epi8(2);
static const __m128i Sevens_i8 = _mm_set1_epi8(7);

static const __m128i AlphaMask0 = _mm_setr_epi32(7 << 0, 0, 7 << 0, 0);
static const __m128i AlphaMask1 = _mm_setr_epi32(7 << 3, 0, 7 << 3, 0);
static const __m128i AlphaMask2 = _mm_setr_epi32(7 << 6, 0, 7 << 6, 0);
static const __m128i AlphaMask3 = _mm_setr_epi32(7 << 9, 0, 7 << 9, 0);
static const __m128i AlphaMask4 = _mm_setr_epi32(7 << 12, 0, 7 << 12, 0);
static const __m128i AlphaMask5 = _mm_setr_epi32(7 << 15, 0, 7 << 15, 0);
static const __m128i AlphaMask6 = _mm_setr_epi32(7 << 18, 0, 7 << 18, 0);
static const __m128i AlphaMask7 = _mm_setr_epi32(7 << 21, 0, 7 << 21, 0); 

void EmitAlphaIndices_SSE2(const uint8_t block[64], const uint8_t min_alpha,
                           const uint8_t max_alpha, uint8_t*& out_data) {
    __m128i line0 = _mm_load_si128(reinterpret_cast<const __m128i*>(block));
    __m128i line1 = _mm_load_si128(reinterpret_cast<const __m128i *>(block + 16));
    __m128i line2 = _mm_load_si128(reinterpret_cast<const __m128i *>(block + 32));
    __m128i line3 = _mm_load_si128(reinterpret_cast<const __m128i *>(block + 48));

    line0 = _mm_srli_epi32(line0, 24);
    line1 = _mm_srli_epi32(line1, 24);
    line2 = _mm_srli_epi32(line2, 24);
    line3 = _mm_srli_epi32(line3, 24);

    __m128i line01 = _mm_packus_epi16(line0, line1);
    __m128i line23 = _mm_packus_epi16(line2, line3);

    __m128i xmm5 = _mm_set1_epi16(max_alpha);
    __m128i xmm7 = xmm5;

    __m128i xmm2 = _mm_set1_epi16(min_alpha);
    __m128i xmm3 = xmm2;

    __m128i xmm4 = xmm5;
    xmm4 = _mm_sub_epi16(xmm4, xmm2);
    xmm4 = _mm_mulhi_epi16(xmm4, DivBy14_i16);

    __m128i xmm1 = xmm2;
    xmm1 = _mm_add_epi16(xmm1, xmm4);
    xmm1 = _mm_packus_epi16(xmm1, xmm1);

    xmm5 = _mm_mullo_epi16(xmm5, ScaleBy_66554400_i16);
    xmm7 = _mm_mullo_epi16(xmm7, ScaleBy_11223300_i16);
    xmm2 = _mm_mullo_epi16(xmm2, ScaleBy_11223300_i16);
    xmm3 = _mm_mullo_epi16(xmm3, ScaleBy_66554400_i16);
    xmm5 = _mm_add_epi16(xmm5, xmm2);
    xmm7 = _mm_add_epi16(xmm7, xmm3);
    xmm5 = _mm_mulhi_epi16(xmm5, DivBy7_i16);
    xmm7 = _mm_mulhi_epi16(xmm7, DivBy7_i16);

    xmm2 = _mm_shuffle_epi32(xmm5, _MM_SHUFFLE(0, 0, 0, 0));
    xmm3 = _mm_shuffle_epi32(xmm5, _MM_SHUFFLE(1, 1, 1, 1));
    xmm4 = _mm_shuffle_epi32(xmm5, _MM_SHUFFLE(2, 2, 2, 2));
    xmm2 = _mm_packus_epi16(xmm2, xmm2);
    xmm3 = _mm_packus_epi16(xmm3, xmm3);
    xmm4 = _mm_packus_epi16(xmm4, xmm4);

    __m128i xmm0 = _mm_packus_epi16(line01, line23);

    xmm5 = _mm_shuffle_epi32(xmm7, _MM_SHUFFLE(2, 2, 2, 2));
    __m128i xmm6 = _mm_shuffle_epi32(xmm7, _MM_SHUFFLE(1, 1, 1, 1));
    xmm7 = _mm_shuffle_epi32(xmm7, _MM_SHUFFLE(0, 0, 0, 0));
    xmm5 = _mm_packus_epi16(xmm5, xmm5);
    xmm6 = _mm_packus_epi16(xmm6, xmm6);
    xmm7 = _mm_packus_epi16(xmm7, xmm7);

    xmm1 = _mm_min_epu8(xmm1, xmm0);
    xmm2 = _mm_min_epu8(xmm2, xmm0);
    xmm3 = _mm_min_epu8(xmm3, xmm0);
    xmm1 = _mm_cmpeq_epi8(xmm1, xmm0);
    xmm2 = _mm_cmpeq_epi8(xmm2, xmm0);
    xmm3 = _mm_cmpeq_epi8(xmm3, xmm0);
    xmm4 = _mm_min_epu8(xmm4, xmm0);
    xmm5 = _mm_min_epu8(xmm5, xmm0);
    xmm6 = _mm_min_epu8(xmm6, xmm0);
    xmm7 = _mm_min_epu8(xmm7, xmm0);
    xmm4 = _mm_cmpeq_epi8(xmm4, xmm0);
    xmm5 = _mm_cmpeq_epi8(xmm5, xmm0);
    xmm6 = _mm_cmpeq_epi8(xmm6, xmm0);
    xmm7 = _mm_cmpeq_epi8(xmm7, xmm0);
    xmm1 = _mm_and_si128(xmm1, Ones_i8);
    xmm2 = _mm_and_si128(xmm2, Ones_i8);
    xmm3 = _mm_and_si128(xmm3, Ones_i8);
    xmm4 = _mm_and_si128(xmm4, Ones_i8);
    xmm5 = _mm_and_si128(xmm5, Ones_i8);
    xmm6 = _mm_and_si128(xmm6, Ones_i8);
    xmm7 = _mm_and_si128(xmm7, Ones_i8);
    xmm0 = Ones_i8;
    xmm0 = _mm_adds_epu8(xmm0, xmm1);
    xmm2 = _mm_adds_epu8(xmm2, xmm3);
    xmm4 = _mm_adds_epu8(xmm4, xmm5);
    xmm6 = _mm_adds_epu8(xmm6, xmm7);
    xmm0 = _mm_adds_epu8(xmm0, xmm2);
    xmm4 = _mm_adds_epu8(xmm4, xmm6);
    xmm0 = _mm_adds_epu8(xmm0, xmm4);
    xmm0 = _mm_and_si128(xmm0, Sevens_i8);
    xmm1 = Twos_i8;
    xmm1 = _mm_cmpgt_epi8(xmm1, xmm0);
    xmm1 = _mm_and_si128(xmm1, Ones_i8);
    xmm0 = _mm_xor_si128(xmm0, xmm1);
    xmm1 = xmm0;
    xmm2 = xmm0;
    xmm3 = xmm0;
    xmm4 = xmm0;
    xmm5 = xmm0;
    xmm6 = xmm0;
    xmm7 = xmm0;
    xmm1 = _mm_srli_epi64(xmm1, 8 - 3);
    xmm2 = _mm_srli_epi64(xmm2, 16 - 6);
    xmm3 = _mm_srli_epi64(xmm3, 24 - 9);
    xmm4 = _mm_srli_epi64(xmm4, 32 - 12);
    xmm5 = _mm_srli_epi64(xmm5, 40 - 15);
    xmm6 = _mm_srli_epi64(xmm6, 48 - 18);
    xmm7 = _mm_srli_epi64(xmm7, 56 - 21);
    xmm0 = _mm_and_si128(xmm0, AlphaMask0);
    xmm1 = _mm_and_si128(xmm1, AlphaMask1);
    xmm2 = _mm_and_si128(xmm2, AlphaMask2);
    xmm3 = _mm_and_si128(xmm3, AlphaMask3);
    xmm4 = _mm_and_si128(xmm4, AlphaMask4);
    xmm5 = _mm_and_si128(xmm5, AlphaMask5);
    xmm6 = _mm_and_si128(xmm6, AlphaMask6);
    xmm7 = _mm_and_si128(xmm7, AlphaMask7);
    xmm0 = _mm_or_si128(xmm0, xmm1);
    xmm2 = _mm_or_si128(xmm2, xmm3);
    xmm4 = _mm_or_si128(xmm4, xmm5);
    xmm6 = _mm_or_si128(xmm6, xmm7);
    xmm0 = _mm_or_si128(xmm0, xmm2);
    xmm4 = _mm_or_si128(xmm4, xmm6);
    xmm0 = _mm_or_si128(xmm0, xmm4);

    _mm_storeu_si32(out_data, xmm0);
    xmm1 = _mm_shuffle_epi32(xmm0, _MM_SHUFFLE(1, 0, 3, 2));
    _mm_storeu_si32(out_data + 3, xmm1);

    out_data += 6;
}

} // namespace Ren

#undef _ABS

#ifdef __GNUC__
#pragma GCC pop_options
#endif