#include "Utils.h"

#include <cassert>

#include <arm_neon.h>

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline
#endif

namespace Ren {
force_inline uint32x4_t _neon_unpacklo_epi8(uint32x4_t a, uint32x4_t b) {
#if defined(__aarch64__)
    return vreinterpretq_u32_u8(
        vzip1q_u8(vreinterpretq_u8_u32(a), vreinterpretq_u8_u32(b)));
#else
    uint8x8_t a1 = vreinterpret_u8_u16(vget_low_u16(vreinterpretq_u8_u32(a)));
    uint8x8_t b1 = vreinterpret_u8_u16(vget_low_u16(vreinterpretq_u8_u32(b)));
    uint8x8x2_t result = vzip_u8(a1, b1);
    return vreinterpretq_u32_u8(vcombine_u8(result.val[0], result.val[1]));
#endif
}

force_inline uint32x4_t _neon_unpackhi_epi8(uint32x4_t a, uint32x4_t b) {
#if defined(__aarch64__)
    return vreinterpretq_u32_u8(
        vzip2q_u8(vreinterpretq_u8_u32(a), vreinterpretq_u8_u32(b)));
#else
    uint8x8_t a1 = vreinterpret_u8_u16(vget_high_u16(vreinterpretq_u8_u32(a)));
    uint8x8_t b1 = vreinterpret_u8_u16(vget_high_u16(vreinterpretq_u8_u32(b)));
    uint8x8x2_t result = vzip_u8(a1, b1);
    return vreinterpretq_u32_u8(vcombine_u8(result.val[0], result.val[1]));
#endif
}
} // namespace Ren

void Ren::CopyYChannel_16px(const uint8_t *y_src, int y_stride, int w, int h,
                            uint8_t *y_dst) {
    const bool is_input_aligned = uintptr_t(y_src) % 16 == 0 && y_stride % 16 == 0;
    const bool is_output_aligned = uintptr_t(y_dst) % 16 == 0;

    if (is_input_aligned && is_output_aligned) {
        auto *py_dst =
            reinterpret_cast<uint32x4_t *>(__builtin_assume_aligned(y_dst, 16));
        for (int y = 0; y < h; ++y) {
            const auto *py_img =
                reinterpret_cast<const uint32x4_t *>(__builtin_assume_aligned(y_src, 16));
            for (int x = 0; x < w / 16; ++x) {
#if __has_builtin(__builtin_nontemporal_store)
                const uint32x4_t y_src_val = __builtin_nontemporal_load(py_img++);
                __builtin_nontemporal_store(y_src_val, py_dst++);
#else
                const uint32x4_t y_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(py_img++));
                vst1q_u32(reinterpret_cast<uint32_t *>(py_dst++), y_src_val);
#endif
            }
            y_src += y_stride;
        }
    } else if (is_input_aligned) {
        auto *py_dst = reinterpret_cast<uint32x4_t *>(y_dst);
        for (int y = 0; y < h; ++y) {
            const auto *py_img =
                reinterpret_cast<const uint32x4_t *>(__builtin_assume_aligned(y_src, 16));
            for (int x = 0; x < w / 16; ++x) {
#if __has_builtin(__builtin_nontemporal_load)
                const uint32x4_t y_src_val = __builtin_nontemporal_load(py_img++);
#else
                const uint32x4_t y_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(py_img++));
#endif
                vst1q_u32(reinterpret_cast<uint32_t *>(py_dst++), y_src_val);
            }
            y_src += y_stride;
        }
    } else if (is_output_aligned) {
        auto *py_dst =
            reinterpret_cast<uint32x4_t *>(__builtin_assume_aligned(y_dst, 16));
        for (int y = 0; y < h; ++y) {
            const auto *py_img = reinterpret_cast<const uint32x4_t *>(y_src);
            for (int x = 0; x < w / 16; ++x) {
                const uint32x4_t y_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(py_img++));
#if __has_builtin(__builtin_nontemporal_store)
                __builtin_nontemporal_store(y_src_val, py_dst++);
#else
                vst1q_u32(reinterpret_cast<uint32_t *>(py_dst++), y_src_val);
#endif
            }
            y_src += y_stride;
        }
    } else {
        auto *py_dst = reinterpret_cast<uint32x4_t *>(y_dst);
        for (int y = 0; y < h; ++y) {
            const auto *py_img = reinterpret_cast<const uint32x4_t *>(y_src);
            for (int x = 0; x < w / 16; ++x) {
                const uint32x4_t y_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(py_img++));
                vst1q_u32(reinterpret_cast<uint32_t *>(py_dst++), y_src_val);
            }
            y_src += y_stride;
        }
    }
}

void Ren::InterleaveUVChannels_16px(const uint8_t *u_src, const uint8_t *v_src,
                                    int u_stride, int v_stride, int w, int h,
                                    uint8_t *uv_dst) {
    const bool is_input_aligned = uintptr_t(u_src) % 16 == 0 && u_stride % 16 == 0 &&
                                  uintptr_t(v_src) % 16 == 0 && v_stride % 16 == 0;
    const bool is_output_aligned = uintptr_t(uv_dst) % 16 == 0;

    if (is_input_aligned && is_output_aligned) {
        auto *puv_dst =
            reinterpret_cast<uint32x4_t *>(__builtin_assume_aligned(uv_dst, 16));
        for (int y = 0; y < h; ++y) {
            const auto *pu_img =
                reinterpret_cast<const uint32x4_t *>(__builtin_assume_aligned(u_src, 16));
            const auto *pv_img =
                reinterpret_cast<const uint32x4_t *>(__builtin_assume_aligned(v_src, 16));
            for (int x = 0; x < w / 16; ++x) {
#if __has_builtin(__builtin_nontemporal_load)
                const uint32x4_t u_src_val = __builtin_nontemporal_load(pu_img++);
                const uint32x4_t v_src_val = __builtin_nontemporal_load(pv_img++);
#else
                const uint32x4_t u_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(pu_img++));
                const uint32x4_t v_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(pv_img++));
#endif
                const uint32x4_t res0 = _neon_unpacklo_epi8(u_src_val, v_src_val);
                const uint32x4_t res1 = _neon_unpackhi_epi8(u_src_val, v_src_val);

#if __has_builtin(__builtin_nontemporal_store)
                __builtin_nontemporal_store(res0, puv_dst++);
                __builtin_nontemporal_store(res1, puv_dst++);
#else
                vst1q_u32(reinterpret_cast<uint32_t *>(puv_dst++), res0);
                vst1q_u32(reinterpret_cast<uint32_t *>(puv_dst++), res1);
#endif
            }
            u_src += u_stride;
            v_src += v_stride;
        }
    } else if (is_input_aligned) {
        auto *puv_dst = reinterpret_cast<uint32x4_t *>(uv_dst);
        for (int y = 0; y < h; ++y) {
            const auto *pu_img =
                reinterpret_cast<const uint32x4_t *>(__builtin_assume_aligned(u_src, 16));
            const auto *pv_img =
                reinterpret_cast<const uint32x4_t *>(__builtin_assume_aligned(v_src, 16));
            for (int x = 0; x < w / 16; ++x) {
#if __has_builtin(__builtin_nontemporal_load)
                const uint32x4_t u_src_val = __builtin_nontemporal_load(pu_img++);
                const uint32x4_t v_src_val = __builtin_nontemporal_load(pv_img++);
#else
                const uint32x4_t u_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(pu_img++));
                const uint32x4_t v_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(pv_img++));
#endif
                const uint32x4_t res0 = _neon_unpacklo_epi8(u_src_val, v_src_val);
                const uint32x4_t res1 = _neon_unpackhi_epi8(u_src_val, v_src_val);

                vst1q_u32(reinterpret_cast<uint32_t *>(puv_dst++), res0);
                vst1q_u32(reinterpret_cast<uint32_t *>(puv_dst++), res1);
            }
            u_src += u_stride;
            v_src += v_stride;
        }
    } else if (is_output_aligned) {
        auto *puv_dst =
            reinterpret_cast<uint32x4_t *>(__builtin_assume_aligned(uv_dst, 16));
        for (int y = 0; y < h; ++y) {
            const auto *pu_img = reinterpret_cast<const uint32x4_t *>(u_src);
            const auto *pv_img = reinterpret_cast<const uint32x4_t *>(v_src);
            for (int x = 0; x < w / 16; ++x) {
                const uint32x4_t u_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(pu_img++));
                const uint32x4_t v_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(pv_img++));

                const uint32x4_t res0 = _neon_unpacklo_epi8(u_src_val, v_src_val);
                const uint32x4_t res1 = _neon_unpackhi_epi8(u_src_val, v_src_val);

                vst1q_u32(reinterpret_cast<uint32_t *>(puv_dst++), res0);
                vst1q_u32(reinterpret_cast<uint32_t *>(puv_dst++), res1);
            }
            u_src += u_stride;
            v_src += v_stride;
        }
    } else {
        auto *puv_dst = reinterpret_cast<uint32x4_t *>(uv_dst);
        for (int y = 0; y < h; ++y) {
            const auto *pu_img = reinterpret_cast<const uint32x4_t *>(u_src);
            const auto *pv_img = reinterpret_cast<const uint32x4_t *>(v_src);
            for (int x = 0; x < w / 16; ++x) {
                const uint32x4_t u_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(pu_img++));
                const uint32x4_t v_src_val =
                    vld1q_u32(reinterpret_cast<const uint32_t *>(pv_img++));

                const uint32x4_t res0 = _neon_unpacklo_epi8(u_src_val, v_src_val);
                const uint32x4_t res1 = _neon_unpackhi_epi8(u_src_val, v_src_val);

                vst1q_u32(reinterpret_cast<uint32_t *>(puv_dst++), res0);
                vst1q_u32(reinterpret_cast<uint32_t *>(puv_dst++), res1);
            }
            u_src += u_stride;
            v_src += v_stride;
        }
    }
}

#undef force_inline
