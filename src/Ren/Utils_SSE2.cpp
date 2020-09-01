#include "Utils.h"

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target("sse2")
#endif

#include <cassert>

#include <immintrin.h>
#include <xmmintrin.h>

#include "CPUFeatures.h"

void Ren::CopyYChannel_16px(const uint8_t *y_src, int y_stride, int w, int h,
                            uint8_t *y_dst) {
    assert(g_CpuFeatures.sse2_supported);
    auto *py_dst = reinterpret_cast<__m128i *>(y_dst);

    const bool is_input_aligned = uintptr_t(y_src) % 16 == 0 && y_stride % 16 == 0;
    const bool is_output_aligned = uintptr_t(py_dst) % 16 == 0;

    if (is_input_aligned && is_output_aligned) {
        if (g_CpuFeatures.sse41_supported) { // for stream load
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m128i *>(y_src);
                for (int x = 0; x < w / 16; ++x) {
                    const __m128i y_src_val = _mm_stream_load_si128(const_cast<__m128i *>(py_img++));
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
                    const __m128i y_src_val = _mm_stream_load_si128(const_cast<__m128i *>(py_img++));
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
                                    int u_stride, int v_stride, int w, int h,
                                    uint8_t *uv_dst) {
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
                    const __m128i u_src_val = _mm_stream_load_si128(const_cast<__m128i *>(pu_img++));
                    const __m128i v_src_val = _mm_stream_load_si128(const_cast<__m128i *>(pv_img++));

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
                    const __m128i u_src_val = _mm_stream_load_si128(const_cast<__m128i *>(pu_img++));
                    const __m128i v_src_val = _mm_stream_load_si128(const_cast<__m128i *>(pv_img++));

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

#ifdef __GNUC__
#pragma GCC pop_options
#endif