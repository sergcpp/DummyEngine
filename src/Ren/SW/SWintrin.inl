#if defined(__aarch64__)
#include <float.h>
#include <arm_neon.h>

#define force_inline __attribute__((always_inline)) inline
#else
#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
#endif

#if defined(USE_SSE2)
#define POSTFIX SSE2
#define SIMD_WIDTH 4

#define SIMD_LANE_NDX _mm_setr_epi32(0, 1, 2, 3)

typedef __m128 __mXXX;
typedef __m128i __mXXXi;

#define _mmXXX_castsiXXX_ps _mm_castsi128_ps
#define _mmXXX_castps_siXXX _mm_castps_si128

#define _mmXXX_setzero_ps _mm_setzero_ps
#define _mmXXX_setzero_siXXX _mm_setzero_si128
#define _mmXXX_set1_ps _mm_set1_ps
#define _mmXXX_setr_ps _mm_setr_ps
#define _mmXXX_set1_epi32 _mm_set1_epi32
#define _mmXXX_setr_epi32 _mm_setr_epi32

#define _mmXXX_add_ps _mm_add_ps
#define _mmXXX_add_epi32 _mm_add_epi32
#define _mmXXX_sub_ps _mm_sub_ps
#define _mmXXX_sub_epi32 _mm_sub_epi32
#define _mmXXX_mul_ps _mm_mul_ps
#define _mmXXX_div_ps _mm_div_ps

#define _mmXXX_neg_ps(a) _mm_xor_ps((a), _mm_set1_ps(-0.0f))

#define _mmXXX_mullo_epi32 _mm_mullo_epi32
#define _mmXXX_subs_epu16 _mm_subs_epu16

#define _mmXXX_min_ps _mm_min_ps
#define _mmXXX_min_epi32 _mm_min_epi32
#define _mmXXX_max_ps _mm_max_ps
#define _mmXXX_max_epi32 _mm_max_epi32

#define _mmXXX_floor_ps(v) _mm_round_ps((v), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)
#define _mmXXX_ceil_ps(v) _mm_round_ps((v), _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)

#define _mmXXX_cvttps_epi32 _mm_cvttps_epi32
#define _mmXXX_cvtepi32_ps _mm_cvtepi32_ps

#define _mmXXX_cmpeq_ps _mm_cmpeq_ps
#define _mmXXX_cmpgt_ps _mm_cmpgt_ps
#define _mmXXX_cmpge_ps _mm_cmpge_ps
#define _mmXXX_cmpeq_epi32 _mm_cmpeq_epi32
#define _mmXXX_cmpgt_epi32 _mm_cmpgt_epi32

#define _mmXXX_not_ps(v) _mm_xor_ps((v), _mm_castsi128_ps(_mm_set1_epi32(~0)))
#define _mmXXX_not_siXXX(a) _mm_xor_si128((a), _mm_set1_epi32(~0))
#define _mmXXX_and_ps _mm_and_ps
#define _mmXXX_and_siXXX _mm_and_si128
#define _mmXXX_andnot_ps _mm_andnot_ps
#define _mmXXX_andnot_siXXX _mm_andnot_si128
#define _mmXXX_or_ps _mm_or_ps
#define _mmXXX_or_siXXX _mm_or_si128

#define _mmXXX_movemask_ps _mm_movemask_ps

#define _mmXXX_fmadd_ps(a, b, c) _mm_add_ps(_mm_mul_ps(a, b), c)
#define _mmXXX_fmsub_ps(a, b, c) _mm_sub_ps(_mm_mul_ps(a, b), c)

#define _mmXXX_slli_epi32 _mm_slli_epi32
#define _mmXXX_srli_epi32 _mm_srli_epi32
#define _mmXXX_srai_epi32 _mm_srai_epi32

#ifdef USE_SSE41
#define _mmXXX_blendv_ps _mm_blendv_ps
#else // USE_SSE41
static __m128 _mmXXX_blendv_ps(const __m128 a, const __m128 b, const __m128 c) {
    __m128 cond = _mm_castsi128_ps(_mm_srai_epi32(_mm_castps_si128(c), 31));
    return _mm_or_ps(_mm_andnot_ps(cond, a), _mm_and_ps(cond, b));
}
#endif // USE_SSE41

#ifdef USE_SSE42
#define _mmXXX_sllv_ones(count) _mm_sllv_epi32(_mm_set1_epi32(0xffffffff), count)
#else // USE_SSE42
static inline __m128i _mmXXX_sllv_ones(const __m128i ishift) {
    union {
        __m128i shift_128;
        uint32_t shift_32[4];
    } shift;

    shift.shift_128 = _mm_min_epi32(ishift, _mm_set1_epi32(32));

    // Uses scalar approach to perform _mm_sllv_epi32(~0, shift)
    static const unsigned int maskLUT[33] = {
        ~0U << 0,  ~0U << 1,  ~0U << 2,  ~0U << 3,  ~0U << 4,  ~0U << 5,  ~0U << 6,
        ~0U << 7,  ~0U << 8,  ~0U << 9,  ~0U << 10, ~0U << 11, ~0U << 12, ~0U << 13,
        ~0U << 14, ~0U << 15, ~0U << 16, ~0U << 17, ~0U << 18, ~0U << 19, ~0U << 20,
        ~0U << 21, ~0U << 22, ~0U << 23, ~0U << 24, ~0U << 25, ~0U << 26, ~0U << 27,
        ~0U << 28, ~0U << 29, ~0U << 30, ~0U << 31, 0U};

    __m128i retMask =
        _mm_setr_epi32(maskLUT[shift.shift_32[0]], maskLUT[shift.shift_32[1]],
                       maskLUT[shift.shift_32[2]], maskLUT[shift.shift_32[3]]);
    return retMask;
}
#endif // USE_SSE42

#ifdef USE_SSE41
static inline __m128i _mmXXX_transpose_epi8(const __m128i a) {
    const __m128i shuff = _mm_setr_epi8(0x0, 0x4, 0x8, 0xC, 0x1, 0x5, 0x9, 0xD, 0x2, 0x6,
                                        0xA, 0xE, 0x3, 0x7, 0xB, 0xF);
    return _mm_shuffle_epi8(a, shuff);
}
#else // USE_SSE41
static inline __m128i _mmXXX_transpose_epi8(__m128i v) {
    // Perform transpose through two 16->8 bit pack and byte shifts
    const __m128i mask =
        _mm_setr_epi8(~0, 0, ~0, 0, ~0, 0, ~0, 0, ~0, 0, ~0, 0, ~0, 0, ~0, 0);
    v = _mm_packus_epi16(_mm_and_si128(v, mask), _mm_srli_epi16(v, 8));
    v = _mm_packus_epi16(_mm_and_si128(v, mask), _mm_srli_epi16(v, 8));
    return v;
}
#endif // USE_SSE41

#ifdef USE_SSE41
#define _mmXXX_testz_siXXX _mm_testz_si128
#else // USE_SSE41
static inline int _mmXXX_testz_siXXX(const __m128i a, const __m128i b) {
    return _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_and_si128(a, b), _mm_setzero_si128())) ==
           0xFFFF;
}
#endif // USE_SSE41

#elif defined(USE_AVX2)
#define POSTFIX AVX2
#define SIMD_WIDTH 8

#define SIMD_LANE_NDX _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7)
#define SIMD_SHUFFLE_SCANLINE_TO_SUBTILES                                                \
    _mm256_setr_epi8(0x0, 0x4, 0x8, 0xC, 0x1, 0x5, 0x9, 0xD, 0x2, 0x6, 0xA, 0xE, 0x3,    \
                     0x7, 0xB, 0xF, 0x0, 0x4, 0x8, 0xC, 0x1, 0x5, 0x9, 0xD, 0x2, 0x6,    \
                     0xA, 0xE, 0x3, 0x7, 0xB, 0xF)

typedef __m256 __mXXX;
typedef __m256i __mXXXi;

#define _mmXXX_castsiXXX_ps _mm256_castsi256_ps
#define _mmXXX_castps_siXXX _mm256_castps_si256

#define _mmXXX_setzero_ps _mm256_setzero_ps
#define _mmXXX_setzero_siXXX _mm256_setzero_si256
#define _mmXXX_set1_ps _mm256_set1_ps
#define _mmXXX_setr_ps _mm256_setr_ps
#define _mmXXX_set1_epi32 _mm256_set1_epi32
#define _mmXXX_setr_epi32 _mm256_setr_epi32

#define _mmXXX_add_ps _mm256_add_ps
#define _mmXXX_add_epi32 _mm256_add_epi32
#define _mmXXX_sub_ps _mm256_sub_ps
#define _mmXXX_sub_epi32 _mm256_sub_epi32
#define _mmXXX_mul_ps _mm256_mul_ps
#define _mmXXX_div_ps _mm256_div_ps

#define _mmXXX_neg_ps(a) _mm256_xor_ps((a), _mm256_set1_ps(-0.0f))

#define _mmXXX_mullo_epi32 _mm256_mullo_epi32
#define _mmXXX_subs_epu16 _mm256_subs_epu16

#define _mmXXX_min_ps _mm256_min_ps
#define _mmXXX_min_epi32 _mm256_min_epi32
#define _mmXXX_max_ps _mm256_max_ps
#define _mmXXX_max_epi32 _mm256_max_epi32

#define _mmXXX_floor_ps _mm256_floor_ps
#define _mmXXX_ceil_ps _mm256_ceil_ps

#define _mmXXX_cvttps_epi32 _mm256_cvttps_epi32
#define _mmXXX_cvtepi32_ps _mm256_cvtepi32_ps

#define _mmXXX_cmpeq_ps(a, b) _mm256_cmp_ps(a, b, _CMP_EQ_OS)
#define _mmXXX_cmpgt_ps(a, b) _mm256_cmp_ps(a, b, _CMP_GT_OS)
#define _mmXXX_cmpge_ps(a, b) _mm256_cmp_ps(a, b, _CMP_GE_OS)
#define _mmXXX_cmpeq_epi32 _mm256_cmpeq_epi32
#define _mmXXX_cmpgt_epi32 _mm256_cmpgt_epi32

#define _mmXXX_not_ps(v) _mm256_xor_ps(v, _mm256_set1_ps(-0.0f))
#define _mmXXX_not_siXXX(a) _mm256_xor_si256((a), _mm256_set1_epi32(~0))
#define _mmXXX_and_ps _mm256_and_ps
#define _mmXXX_and_siXXX _mm256_and_si256
#define _mmXXX_andnot_ps _mm256_andnot_ps
#define _mmXXX_andnot_siXXX _mm256_andnot_si256
#define _mmXXX_or_ps _mm256_or_ps
#define _mmXXX_or_siXXX _mm256_or_si256

#define _mmXXX_movemask_ps _mm256_movemask_ps

#define _mmXXX_fmadd_ps _mm256_fmadd_ps
#define _mmXXX_fmsub_ps _mm256_fmsub_ps

#define _mmXXX_slli_epi32 _mm256_slli_epi32
#define _mmXXX_srli_epi32 _mm256_srli_epi32
#define _mmXXX_srai_epi32 _mm256_srai_epi32

#define _mmXXX_blendv_ps _mm256_blendv_ps

#define _mmXXX_sllv_ones(count) _mm256_sllv_epi32(_mm256_set1_epi32(0xffffffff), count)

#define _mmXXX_transpose_epi8(x) _mm256_shuffle_epi8(x, SIMD_SHUFFLE_SCANLINE_TO_SUBTILES)

#define _mmXXX_testz_siXXX _mm256_testz_si256

#elif defined(USE_AVX512)
#define POSTFIX AVX512
#define SIMD_WIDTH 16

#define SIMD_LANE_NDX                                                                    \
    _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)
#define SIMD_SHUFFLE_SCANLINE_TO_SUBTILES                                                \
    _mm512_set_epi32(0x0F0B0703, 0x0E0A0602, 0x0D090501, 0x0C080400, 0x0F0B0703,         \
                     0x0E0A0602, 0x0D090501, 0x0C080400, 0x0F0B0703, 0x0E0A0602,         \
                     0x0D090501, 0x0C080400, 0x0F0B0703, 0x0E0A0602, 0x0D090501,         \
                     0x0C080400)

typedef __m512 __mXXX;
typedef __m512i __mXXXi;

#define _mmXXX_castsiXXX_ps _mm512_castsi512_ps
#define _mmXXX_castps_siXXX _mm512_castps_si512

#define _mmXXX_setzero_ps _mm512_setzero_ps
#define _mmXXX_setzero_siXXX _mm512_setzero_si512
#define _mmXXX_set1_ps _mm512_set1_ps
#define _mmXXX_setr_ps _mm512_setr_ps
#define _mmXXX_set1_epi32 _mm512_set1_epi32
#define _mmXXX_setr_epi32 _mm512_setr_epi32

#define _mmXXX_add_ps _mm512_add_ps
#define _mmXXX_add_epi32 _mm512_add_epi32
#define _mmXXX_sub_ps _mm512_sub_ps
#define _mmXXX_sub_epi32 _mm512_sub_epi32
#define _mmXXX_mul_ps _mm512_mul_ps
#define _mmXXX_div_ps _mm512_div_ps

#define _mmXXX_neg_ps(a) _mm512_xor_ps((a), _mm512_set1_ps(-0.0f))

#define _mmXXX_mullo_epi32 _mm512_mullo_epi32
#define _mmXXX_subs_epu16 _mm512_subs_epu16

#define _mmXXX_min_ps _mm512_min_ps
#define _mmXXX_min_epi32 _mm512_min_epi32
#define _mmXXX_max_ps _mm512_max_ps
#define _mmXXX_max_epi32 _mm512_max_epi32

#define _mmXXX_floor_ps _mm512_floor_ps
#define _mmXXX_ceil_ps _mm512_ceil_ps

#define _mmXXX_cvttps_epi32 _mm512_cvttps_epi32
#define _mmXXX_cvtepi32_ps _mm512_cvtepi32_ps

static __m512 _mmXXX_cmpeq_ps(__m512 a, __m512 b) {
    __mmask16 mask = _mm512_cmp_ps_mask(a, b, _CMP_EQ_OQ);
    return _mm512_castsi512_ps(
        _mm512_mask_mov_epi32(_mm512_set1_epi32(0), mask, _mm512_set1_epi32(~0)));
}
#define _mmXXX_not_siXXX(a) _mm512_xor_si512((a), _mm512_set1_epi32(~0))

static __m512 _mmXXX_cmpgt_ps(__m512 a, __m512 b) {
    __mmask16 mask = _mm512_cmp_ps_mask(a, b, _CMP_GT_OQ);
    return _mm512_castsi512_ps(
        _mm512_mask_mov_epi32(_mm512_set1_epi32(0), mask, _mm512_set1_epi32(~0)));
}

static __m512 _mmXXX_cmpge_ps(__m512 a, __m512 b) {
    __mmask16 mask = _mm512_cmp_ps_mask(a, b, _CMP_GE_OQ);
    return _mm512_castsi512_ps(
        _mm512_mask_mov_epi32(_mm512_set1_epi32(0), mask, _mm512_set1_epi32(~0)));
}

static __m512i _mmXXX_cmpeq_epi32(__m512i a, __m512i b) {
    __mmask16 mask = _mm512_cmpeq_epi32_mask(a, b);
    return _mm512_mask_mov_epi32(_mm512_set1_epi32(0), mask, _mm512_set1_epi32(~0));
}

static __m512i _mmXXX_cmpgt_epi32(__m512i a, __m512i b) {
    __mmask16 mask = _mm512_cmpgt_epi32_mask(a, b);
    return _mm512_mask_mov_epi32(_mm512_set1_epi32(0), mask, _mm512_set1_epi32(~0));
}

#define _mmXXX_not_ps(v) _mm512_xor_ps(v, _mm512_set1_ps(-0.0f))
#define _mmXXX_and_ps _mm512_and_ps
#define _mmXXX_and_siXXX _mm512_and_si512
#define _mmXXX_andnot_ps _mm512_andnot_ps
#define _mmXXX_andnot_siXXX _mm512_andnot_si512
#define _mmXXX_or_ps _mm512_or_ps
#define _mmXXX_or_siXXX _mm512_or_si512

static __mmask16 _mmXXX_movemask_ps(const __m512 a) {
    __mmask16 mask = _mm512_cmp_epi32_mask(
        _mm512_and_si512(_mm512_castps_si512(a), _mm512_set1_epi32(0x80000000)),
        _mm512_set1_epi32(0), 4); // a & 0x8000000 != 0
    return mask;
}

#define _mmXXX_fmadd_ps _mm512_fmadd_ps
#define _mmXXX_fmsub_ps _mm512_fmsub_ps

#define _mmXXX_slli_epi32 _mm512_slli_epi32
#define _mmXXX_srli_epi32 _mm512_srli_epi32
#define _mmXXX_srai_epi32 _mm512_srai_epi32

static __m512 _mmXXX_blendv_ps(const __m512 a, const __m512 b, const __m512 c) {
    __mmask16 mask = _mmXXX_movemask_ps(c);
    return _mm512_mask_mov_ps(a, mask, b);
}

#define _mmXXX_sllv_ones(count) _mm512_sllv_epi32(_mm512_set1_epi32(0xffffffff), count)

#define _mmXXX_transpose_epi8(x) _mm512_shuffle_epi8(x, SIMD_SHUFFLE_SCANLINE_TO_SUBTILES)

static inline int _mmXXX_testz_siXXX(__m512i a, __m512i b) {
    __mmask16 mask =
        _mm512_cmpeq_epi32_mask(_mm512_and_si512(a, b), _mm512_set1_epi32(0));
    return mask == 0xFFFF;
}

#elif defined(__aarch64__)
#define POSTFIX NEON
#define SIMD_WIDTH 4

#define SIMD_LANE_NDX (int32x4_t){ 0, 1, 2, 3 }

typedef float32x4_t __mXXX;
typedef int32x4_t __mXXXi;

#define _mmXXX_castsiXXX_ps vreinterpretq_f32_s32
#define _mmXXX_castps_siXXX vreinterpretq_s32_f32

#define _mmXXX_setzero_ps() vdupq_n_f32(0)
#define _mmXXX_setzero_siXXX() vdupq_n_s32(0)
#define _mmXXX_set1_ps(v) vdupq_n_f32(v)

force_inline float32x4_t _mmXXX_setr_ps(float w, float z, float y, float x) {
    float __attribute__((aligned(16))) data[4] = {w, z, y, x};
    return vld1q_f32(data);
}

#define _mmXXX_set1_epi32 vdupq_n_s32

force_inline int32x4_t _mmXXX_setr_epi32(int32_t w, int32_t z, int32_t y, int32_t x) {
    int32_t __attribute__((aligned(16))) data[4] = {w, z, y, x};
    return vld1q_s32(data);
}

#define _mmXXX_add_ps vaddq_f32
#define _mmXXX_add_epi32 vaddq_s32
#define _mmXXX_sub_ps vsubq_f32
#define _mmXXX_sub_epi32 vsubq_s32
#define _mmXXX_mul_ps vmulq_f32
#define _mmXXX_div_ps vdivq_f32

#define _mmXXX_xor_ps(a, b) vreinterpretq_f32_s32(veorq_s32(vreinterpretq_s32_f32(a), vreinterpretq_s32_f32(b)))
#define _mmXXX_neg_ps(a) _mmXXX_xor_ps((a), vdupq_n_f32(-0.0f))

#define _mmXXX_mullo_epi32 vmulq_s32
#define _mmXXX_subs_epu16(a, b)                                                          \
    vreinterpretq_s32_u16(vqsubq_u16(vreinterpretq_u16_s32(a), vreinterpretq_u16_s32(b)))

#define _mmXXX_min_ps vminq_f32
#define _mmXXX_min_epi32 vminq_s32
#define _mmXXX_max_ps vmaxq_f32
#define _mmXXX_max_epi32 vmaxq_s32

#define _mmXXX_floor_ps vrndmq_f32
#define _mmXXX_ceil_ps vrndpq_f32

#define _mmXXX_cvttps_epi32 vcvtq_s32_f32
#define _mmXXX_cvtepi32_ps vcvtq_f32_s32

#define _mmXXX_cmpeq_ps vceqq_f32
#define _mmXXX_cmpgt_ps vcgtq_f32
#define _mmXXX_cmpge_ps vcgeq_f32
#define _mmXXX_cmpeq_epi32 vceqq_s32
#define _mmXXX_cmpgt_epi32 vcgtq_s32

#define _mmXXX_not_ps(v) _mmXXX_xor_ps((v), vreinterpretq_f32_s32(vdupq_n_s32(~0)))
#define _mmXXX_not_siXXX(a) veorq_s32((a), vdupq_n_s32(~0))
#define _mmXXX_and_ps(a, b) vreinterpretq_f32_s32(vandq_s32(vreinterpretq_s32_f32(a), vreinterpretq_s32_f32(b)))
#define _mmXXX_and_siXXX vandq_s32
#define _mmXXX_andnot_ps(a, b) vreinterpretq_f32_s32(vbicq_s32(vreinterpretq_s32_f32(b), vreinterpretq_s32_f32(a)))
#define _mmXXX_andnot_siXXX(a, b) vbicq_s32(b, a)
#define _mmXXX_or_ps(a, b) vreinterpretq_f32_s32(vorrq_s32(vreinterpretq_s32_f32(a), vreinterpretq_s32_f32(b)))
#define _mmXXX_or_siXXX vorrq_s32

force_inline int _mmXXX_movemask_ps(float32x4_t a) {
    uint32x4_t input = vreinterpretq_u32_f32(a);
#if defined(__aarch64__)
    static const int32x4_t shift = {0, 1, 2, 3};
    uint32x4_t tmp = vshrq_n_u32(input, 31);
    return vaddvq_u32(vshlq_u32(tmp, shift));
#else
    // Uses the exact same method as _mm_movemask_epi8, see that for details.
    // Shift out everything but the sign bits with a 32-bit unsigned shift
    // right.
    uint64x2_t high_bits = vreinterpretq_u64_u32(vshrq_n_u32(input, 31));
    // Merge the two pairs together with a 64-bit unsigned shift right + add.
    uint8x16_t paired =
        vreinterpretq_u8_u64(vsraq_n_u64(high_bits, high_bits, 31));
    // Extract the result.
    return vgetq_lane_u8(paired, 0) | (vgetq_lane_u8(paired, 8) << 2);
#endif
}

#define _mmXXX_fmadd_ps(a, b, c) vaddq_f32(vmulq_f32(a, b), c)
#define _mmXXX_fmsub_ps(a, b, c) vsubq_f32(vmulq_f32(a, b), c)

#define _likely(x) __builtin_expect(!!(x), 1)
#define _unlikely(x) __builtin_expect(!!(x), 0)

force_inline float32x4_t _mmXXX_slli_epi32(int32x4_t a, int imm) {
    if (_unlikely(imm & ~31)) {
        return vdupq_n_f32(0);
    }
    return vreinterpretq_f32_s32(vshlq_s32(a, vdupq_n_s32(imm)));
}

#define _mmXXX_srli_epi32(a, imm)                                            \
    __extension__({                                                          \
        int32x4_t ret;                                                       \
        if (_unlikely((imm) & ~31)) {                                        \
            ret = vdupq_n_s32(0);                                            \
        } else {                                                             \
            ret = vreinterpretq_s32_u32(                                     \
                vshlq_u32(vreinterpretq_u32_s32(a), vdupq_n_s32(-(imm))));   \
        }                                                                    \
        ret;                                                                 \
    })

#define _mmXXX_srai_epi32(a, imm)                                            \
    __extension__({                                                          \
        int32x4_t ret;                                                       \
        if (0 < (imm) && (imm) < 32) {                                       \
            ret = vshlq_s32(a, vdupq_n_s32(-imm));                           \
        } else {                                                             \
            ret = vshrq_n_s32(a, 31);                                        \
        }                                                                    \
        ret;                                                                 \
    })

force_inline int32x4_t _mmXXX_sllv_ones(const int32x4_t ishift) {
    union {
        int32x4_t shift_128;
        uint32_t shift_32[4];
    } shift;

    shift.shift_128 = _mmXXX_min_epi32(ishift, _mmXXX_set1_epi32(32));

    // Uses scalar approach to perform _mm_sllv_epi32(~0, shift)
    static const unsigned int maskLUT[33] = {
        ~0U << 0,  ~0U << 1,  ~0U << 2,  ~0U << 3,  ~0U << 4,  ~0U << 5,  ~0U << 6,
        ~0U << 7,  ~0U << 8,  ~0U << 9,  ~0U << 10, ~0U << 11, ~0U << 12, ~0U << 13,
        ~0U << 14, ~0U << 15, ~0U << 16, ~0U << 17, ~0U << 18, ~0U << 19, ~0U << 20,
        ~0U << 21, ~0U << 22, ~0U << 23, ~0U << 24, ~0U << 25, ~0U << 26, ~0U << 27,
        ~0U << 28, ~0U << 29, ~0U << 30, ~0U << 31, 0U};

    int32x4_t retMask =
        _mmXXX_setr_epi32(maskLUT[shift.shift_32[0]], maskLUT[shift.shift_32[1]],
                          maskLUT[shift.shift_32[2]], maskLUT[shift.shift_32[3]]);
    return retMask;
}

force_inline float32x4_t _mmXXX_blendv_ps(float32x4_t a, float32x4_t b, float32x4_t _mask) {
    // Use a signed shift right to create a mask with the sign bit
    uint32x4_t mask = vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_f32(_mask), 31));
    return vbslq_f32(mask, b, a);
}

force_inline int32x4_t _mm_shuffle_epi8(int32x4_t a, int32x4_t b) {
    int8x16_t tbl = vreinterpretq_s8_s32(a);   // input a
    uint8x16_t idx = vreinterpretq_u8_s32(b);  // input b
    uint8x16_t idx_masked = vandq_u8(idx, vdupq_n_u8(0x8F));  // avoid using meaningless bits
#if defined(__aarch64__)
    return vreinterpretq_s32_s8(vqtbl1q_s8(tbl, idx_masked));
#elif defined(__GNUC__)
    int8x16_t ret;
    // %e and %f represent the even and odd D registers
    // respectively.
    __asm__ __volatile__(
        "vtbl.8  %e[ret], {%e[tbl], %f[tbl]}, %e[idx]\n"
        "vtbl.8  %f[ret], {%e[tbl], %f[tbl]}, %f[idx]\n"
        : [ret] "=&w"(ret)
        : [tbl] "w"(tbl), [idx] "w"(idx_masked));
    return vreinterpretq_m128i_s8(ret);
#else
    // use this line if testing on aarch64
    int8x8x2_t a_split = {vget_low_s8(tbl), vget_high_s8(tbl)};
    return vreinterpretq_m128i_s8(
        vcombine_s8(vtbl2_s8(a_split, vget_low_u8(idx_masked)),
                    vtbl2_s8(a_split, vget_high_u8(idx_masked))));
#endif
}

force_inline int32x4_t _mmXXX_transpose_epi8(int32x4_t v) {
    // Perform transpose through two 16->8 bit pack and byte shifts
    int8_t __attribute__((aligned(16)))
            data[16] = {~0, 0, ~0, 0, ~0, 0, ~0, 0, ~0, 0, ~0, 0, ~0, 0, ~0, 0};
    const int32x4_t mask = vld1q_s8(data);
    
#define _mm_srli_epi16(a, imm)                                               \
    __extension__({                                                          \
        int32x4_t ret;                                                       \
        if (_unlikely((imm) & ~15)) {                                        \
            ret = vdupq_n_s32(0);                                            \
        } else {                                                             \
            ret = vreinterpretq_s32_u16(                                     \
                vshlq_u16(vreinterpretq_u16_s32(a), vdupq_n_s16(-(imm))));   \
        }                                                                    \
        ret;                                                                 \
    })
    
#define _mm_packus_epi16(a, b)                                               \
    vreinterpretq_s32_u8(                                                    \
            vcombine_u8(vqmovun_s16(vreinterpretq_s16_s32(a)),               \
                        vqmovun_s16(vreinterpretq_s16_s32(b))));
    
    v = _mm_packus_epi16(vandq_s32(v, mask), _mm_srli_epi16(v, 8));
    v = _mm_packus_epi16(vandq_s32(v, mask), _mm_srli_epi16(v, 8));
    
#undef _mm_packus_epi16
#undef _mm_srli_epi16
    
    return v;
}

force_inline int _mmXXX_testz_siXXX(int32x4_t a, int32x4_t b) {
    int64x2_t s64 =
        vandq_s64(vreinterpretq_s64_s32(a), vreinterpretq_s64_s32(b));
    return !(vgetq_lane_s64(s64, 0) | vgetq_lane_s64(s64, 1));
}

#endif

#if !defined(__aarch64__)
#define _mm128_setzero_si128 _mm_setzero_si128
#define _mm128_set1_ps _mm_set1_ps
#define _mm128_setr_ps _mm_setr_ps
#define _mm128_setr_epi32 _mm_setr_epi32

#define _mm128_add_ps _mm_add_ps
#define _mm128_sub_ps _mm_sub_ps
#define _mm128_mul_ps _mm_mul_ps
#define _mm128_div_ps _mm_div_ps

#define _mm128_fmadd_ps(a, b, c) _mm_add_ps(_mm_mul_ps(a, b), c)

#define _mm128_add_epi32 _mm_add_epi32

#define _mm128_and_si128 _mm_and_si128

#define _mm128_min_epi32 _mm_min_epi32
#define _mm128_max_epi32 _mm_max_epi32

#define _mm128_xor_ps _mm_xor_ps

#define _mm128_movemask_ps _mm_movemask_ps

#define _mm128_cvtps_epi32 _mm_cvtps_epi32

static inline __m128 _mm128_dp4_ps(const __m128 a, const __m128 b) {
    __m128 prod = _mm_mul_ps(a, b);
    __m128 dp = _mm_add_ps(prod, _mm_shuffle_ps(prod, prod, _MM_SHUFFLE(2, 3, 0, 1)));
    dp = _mm_add_ps(dp, _mm_shuffle_ps(dp, dp, _MM_SHUFFLE(0, 1, 2, 3)));
    return dp;
}
#else
typedef float32x4_t __m128;
typedef int32x4_t __m128i;

#define _mm128_setzero_si128() vdupq_n_s32(0)
#define _mm128_set1_ps vdupq_n_f32
#define _mm128_setr_ps _mmXXX_setr_ps
#define _mm128_setr_epi32 _mmXXX_setr_epi32

static void __test() {
    int32x4_t a = _mm128_setr_epi32(0, 1, 2, 3);
}

#define _mm128_add_ps vaddq_f32
#define _mm128_sub_ps vsubq_f32
#define _mm128_mul_ps vmulq_f32
#define _mm128_div_ps vdivq_f32

#define _mm128_fmadd_ps(a, b, c) vaddq_f32(vmulq_f32(a, b), c)

#define _mm128_add_epi32 vaddq_s32

#define _mm128_and_si128 vandq_s32

#define _mm128_min_epi32 vminq_s32
#define _mm128_max_epi32 vmaxq_s32

#define _mm128_xor_ps(a, b) vreinterpretq_f32_s32(veorq_s32(vreinterpretq_s32_f32(a), vreinterpretq_s32_f32(b)))

#define _mm128_movemask_ps _mmXXX_movemask_ps

#define _mm128_cvtps_epi32 vcvtmq_s32_f32

force_inline float32x4_t _mm_shuffle_ps_2301(float32x4_t a, float32x4_t b) {
    float32x2_t a01 = vrev64_f32(vget_low_f32(a));
    float32x2_t b23 = vrev64_f32(vget_high_f32(b));
    return vcombine_f32(a01, b23);
}

force_inline float32x4_t _mm128_dp4_ps(const float32x4_t a, const float32x4_t b) {
    float32x4_t prod = vmulq_f32(a, b);
    float32x4_t dp = vaddq_f32(prod, _mm_shuffle_ps_2301(prod, prod));
    
    float32x4_t v = vrev64q_f32(dp);
    v = vextq_f32(v, v, 2);
    
    dp = vaddq_f32(dp, v);
    return dp;
}
#endif

#undef force_inline
