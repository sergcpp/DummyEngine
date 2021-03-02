#include "Utils.h"

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target("sse2")
#endif

#include <cassert>
#include <cstring>

#include <immintrin.h>
#include <xmmintrin.h>

#include "CPUFeatures.h"

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

// WARNING: Reads 4 bytes outside of block!
namespace Ren {
void Extract4x4Block_SSSE3(const uint8_t src[], const int stride, const int channels,
                           uint8_t dst[64]) {
    // clang-format off
    // Copy rgb values and zero out alpha
    static const __m128i RGB_to_RGBA = _mm_set_epi8(-1 /* Insert zero */, 11, 10, 9,
        -1 /* Insert zero */, 8, 7, 6,
        -1 /* Insert zero */, 5, 4, 3,
        -1 /* Insert zero */, 2, 1, 0);
    // clang-format on

    if (channels == 4) {
        for (int j = 0; j < 4; j++) {
            const __m128i rgba = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src));
            _mm_store_si128(reinterpret_cast<__m128i *>(dst), rgba);

            src += stride;
            dst += 4 * 4;
        }
    } else if (channels == 3) {
        for (int j = 0; j < 4; j++) {
            const __m128i rgb = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src));
            const __m128i rgba = _mm_shuffle_epi8(rgb, RGB_to_RGBA);
            _mm_store_si128(reinterpret_cast<__m128i *>(dst), rgba);

            src += stride;
            dst += 4 * 4;
        }
    }
}

void GetMinMaxColorByBBox_SSE41(const uint8_t block[64], uint8_t min_color[4],
                                uint8_t max_color[4]) {
    __m128i min_col = _mm_set1_epi8(-1 /* 255 */);
    __m128i max_col = _mm_set1_epi8(0);

    for (int i = 0; i < 4; i++) {
        const __m128i cur_col =
            _mm_load_si128(reinterpret_cast<const __m128i *>(&block[i * 16]));

        min_col = _mm_min_epu8(min_col, cur_col);
        max_col = _mm_max_epu8(max_col, cur_col);
    }

    // Find horizontal min/max values
    min_col = _mm_min_epu8(min_col, _mm_srli_si128(min_col, 8));
    min_col = _mm_min_epu8(min_col, _mm_srli_si128(min_col, 4));

    max_col = _mm_max_epu8(max_col, _mm_srli_si128(max_col, 8));
    max_col = _mm_max_epu8(max_col, _mm_srli_si128(max_col, 4));

#if 1 // this is more safe
    int temp = _mm_cvtsi128_si32(min_col);
    memcpy(min_color, &temp, 4);
    temp = _mm_cvtsi128_si32(max_col);
    memcpy(max_color, &temp, 4);
#else
    *reinterpret_cast<uint32_t *>(min_color) = _mm_cvtsi128_si32(min_col);
    *reinterpret_cast<uint32_t *>(max_color) = _mm_cvtsi128_si32(max_col);
#endif
}
} // namespace Ren

#ifdef __GNUC__
#pragma GCC pop_options
#endif