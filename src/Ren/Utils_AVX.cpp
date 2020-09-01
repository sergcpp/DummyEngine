#include "Utils.h"

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target("avx")
#endif

#include <cassert>

#include <immintrin.h>
#include <xmmintrin.h>

#include "CPUFeatures.h"

void Ren::CopyYChannel_32px(const uint8_t *y_src, int y_stride, int w, int h,
                            uint8_t *y_dst) {
    assert(g_CpuFeatures.avx_supported);
    auto *py_dst = reinterpret_cast<__m256i *>(y_dst);

    const bool is_input_aligned = uintptr_t(y_src) % 32 == 0 && y_stride % 32 == 0;
    const bool is_output_aligned = uintptr_t(py_dst) % 32 == 0;

    if (is_input_aligned && is_output_aligned) {
        if (g_CpuFeatures.avx2_supported) { // for stream load
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m256i *>(y_src);
                for (int x = 0; x < w / 32; ++x) {
                    const __m256i y_src_val = _mm256_stream_load_si256(py_img++);
                    _mm256_stream_si256(py_dst++, y_src_val);
                }
                y_src += y_stride;
            }
        } else {
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m256i *>(y_src);
                for (int x = 0; x < w / 32; ++x) {
                    const __m256i y_src_val = _mm256_load_si256(py_img++);
                    _mm256_stream_si256(py_dst++, y_src_val);
                }
                y_src += y_stride;
            }
        }
    } else if (is_input_aligned) {
        if (g_CpuFeatures.avx2_supported) { // for stream load
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m256i *>(y_src);
                for (int x = 0; x < w / 32; ++x) {
                    const __m256i y_src_val = _mm256_stream_load_si256(py_img++);
                    _mm256_storeu_si256(py_dst++, y_src_val);
                }
                y_src += y_stride;
            }
        } else {
            for (int y = 0; y < h; ++y) {
                const auto *py_img = reinterpret_cast<const __m256i *>(y_src);
                for (int x = 0; x < w / 32; ++x) {
                    const __m256i y_src_val = _mm256_load_si256(py_img++);
                    _mm256_storeu_si256(py_dst++, y_src_val);
                }
                y_src += y_stride;
            }
        }
    } else if (is_output_aligned) {
        for (int y = 0; y < h; ++y) {
            const auto *py_img = reinterpret_cast<const __m256i *>(y_src);
            for (int x = 0; x < w / 32; ++x) {
                const __m256i y_src_val = _mm256_loadu_si256(py_img++);
                _mm256_stream_si256(py_dst++, y_src_val);
            }
            y_src += y_stride;
        }
    } else {
        for (int y = 0; y < h; ++y) {
            const auto *py_img = reinterpret_cast<const __m256i *>(y_src);
            for (int x = 0; x < w / 32; ++x) {
                const __m256i y_src_val = _mm256_loadu_si256(py_img++);
                _mm256_storeu_si256(py_dst++, y_src_val);
            }
            y_src += y_stride;
        }
    }
}

#ifdef __GNUC__
#pragma GCC pop_options
#endif