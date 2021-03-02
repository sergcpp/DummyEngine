#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>

#if defined(USE_SSE2)
#define POSTFIX SSE2
#define SIMD_WIDTH 4

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

#define _mmXXX_cmpeq_ps _mm_cmpeq_ps
#define _mmXXX_cmpgt_ps _mm_cmpgt_ps
#define _mmXXX_cmpeq_epi32 _mm_cmpeq_epi32

#define _mmXXX_not_ps(v) _mm_xor_ps((v), _mm_castsi128_ps(_mm_set1_epi32(~0)))
#define _mmXXX_not_siXXX(a) _mm_xor_si128((a), _mm_set1_epi32(~0))
#define _mmXXX_and_ps _mm_and_ps
#define _mmXXX_and_siXXX _mm_and_si128
#define _mmXXX_andnot_ps _mm_andnot_ps
#define _mmXXX_andnot_siXXX _mm_andnot_si128
#define _mmXXX_or_ps _mm_or_ps
#define _mmXXX_or_siXXX _mm_or_si128

#define _mmXXX_movemask_ps _mm_movemask_ps

#define _mmXXX_fmadd_ps(a,b,c) _mm_add_ps(_mm_mul_ps(a,b), c)

#define _mmXXX_slli_epi32 _mm_slli_epi32
#define _mmXXX_srli_epi32 _mm_srli_epi32
#define _mmXXX_srai_epi32 _mm_srai_epi32

#ifdef USE_SSE41
#define _mmXXX_blendv_ps _mm_blendv_ps
#else
static __m128 _mmXXX_blendv_ps(const __m128 a, const __m128 b, const __m128 c) {
    __m128 cond = _mm_castsi128_ps(_mm_srai_epi32(_mm_castps_si128(c), 31));
    return _mm_or_ps(_mm_andnot_ps(cond, a), _mm_and_ps(cond, b));
}
#endif

#ifdef USE_SSE42
#define _mmXXX_sllv_ones(count) _mm_sllv_epi32(_mm_set1_epi32(0xffffffff), count)
#else
static __m128i _mmXXX_sllv_ones(const __m128i ishift) {
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

#endif

#define SIMD_LANE_NDX _mm_setr_epi32(0, 1, 2, 3)

#elif defined(USE_AVX2)
#define POSTFIX AVX2
#define SIMD_WIDTH 8

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

#define _mmXXX_cmpeq_ps(a, b) _mm256_cmp_ps(a, b, _CMP_EQ_OS)
#define _mmXXX_cmpgt_ps(a, b) _mm256_cmp_ps(a, b, _CMP_GT_OS)
#define _mmXXX_cmpeq_epi32 _mm256_cmpeq_epi32

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

#define _mmXXX_slli_epi32 _mm256_slli_epi32
#define _mmXXX_srli_epi32 _mm256_srli_epi32
#define _mmXXX_srai_epi32 _mm256_srai_epi32

#define _mmXXX_blendv_ps _mm256_blendv_ps

#define _mmXXX_sllv_ones(count) _mm256_sllv_epi32(_mm256_set1_epi32(0xffffffff), count)

#define SIMD_LANE_NDX _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7)

#elif defined(USE_AVX512)
#define POSTFIX AVX512
#define SIMD_WIDTH 16

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

static __m512 _mmXXX_cmpeq_ps(__m512 a, __m512 b) {
    __mmask16 mask = _mm512_cmp_ps_mask(a, b, _CMP_EQ_OQ);
    return _mm512_castsi512_ps(_mm512_mask_mov_epi32(_mm512_set1_epi32(0), mask, _mm512_set1_epi32(~0)));
}
#define _mmXXX_not_siXXX(a) _mm512_xor_si512((a), _mm512_set1_epi32(~0))

static __m512 _mmXXX_cmpgt_ps(__m512 a, __m512 b) {
    __mmask16 mask = _mm512_cmp_ps_mask(a, b, _CMP_GT_OQ);
    return _mm512_castsi512_ps(_mm512_mask_mov_epi32(_mm512_set1_epi32(0), mask, _mm512_set1_epi32(~0)));
}

static __m512i _mmXXX_cmpeq_epi32(__m512i a, __m512i b) {
    __mmask16 mask = _mm512_cmpeq_epi32_mask(a, b);
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
    __mmask16 mask = _mm512_cmp_epi32_mask(_mm512_and_si512(_mm512_castps_si512(a), _mm512_set1_epi32(0x80000000)), _mm512_set1_epi32(0), 4);	// a & 0x8000000 != 0
    return mask;
}

#define _mmXXX_fmadd_ps _mm512_fmadd_ps

#define _mmXXX_slli_epi32 _mm512_slli_epi32
#define _mmXXX_srli_epi32 _mm512_srli_epi32
#define _mmXXX_srai_epi32 _mm512_srai_epi32

static __m512 _mmXXX_blendv_ps(const __m512 a, const __m512 b, const __m512 c) {
    __mmask16 mask = _mmXXX_movemask_ps(c);
    return _mm512_mask_mov_ps(a, mask, b);
}

#define _mmXXX_sllv_ones(count) _mm512_sllv_epi32(_mm512_set1_epi32(0xffffffff), count)

#define SIMD_LANE_NDX _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)

#elif defined(USE_NEON)
#define POSTFIX NEON
#define SIMD_WIDTH 4

typedef float32x4_t __mXXX;
typedef int32x4_t __mXXXi;

#define _mmXXX_set1_epi32 vdupq_n_s32
#define _mmXXX_setr_epi32(i1, i2, i3, i4) int32x4_t{ i1, i2, i3, i4 }

#define _mmXXX_add_epi32 vaddq_s32
#define _mmXXX_sub_epi32 vsubq_s32

#define _mmXXX_mullo_epi32 vmulq_s32
#define _mmXXX_subs_epu16(a, b) vreinterpretq_s32_u16(vqsubq_u16(vreinterpretq_u16_s32(a), vreinterpretq_u16_s32(b)))

#define _mmXXX_min_epi32 vminq_s32
#define _mmXXX_max_epi32 vmaxq_s32

#define _mmXXX_and_epi32 vandq_s32
#define _mmXXX_andnot_epi32 vbicq_s32
#define _mmXXX_or_epi32 vorrq_s32

#define _mmXXX_srai_epi32(a, imm)                                         \
    __extension__({                                                     \
        int32x4_t ret;                                                  \
        if (0 < (imm) && (imm) < 32) {                                  \
            ret = vshlq_s32(a, vdupq_n_s32(-imm)));                     \
        } else {                                                        \
            ret = vshrq_n_s32(a, 31));                                  \
        }                                                               \
        ret;                                                            \
    })

#define _mmXXX_sllv_ones(count) _mm256_sllv_epi32(_mm256_set1_epi32(0xffffffff), count)

static int32x4_t _mmXXX_sllv_ones(const int32x4_t ishift) {
    union {
        int32x4_t shift_128;
        uint32_t shift_32[4];
    } shift;

    shift.shift_128 = _mm_min_epi32(ishift, _mm_set1_epi32(32));

    // Uses scalar approach to perform _mm_sllv_epi32(~0, shift)
    static const unsigned int maskLUT[33] = {
        ~0U << 0,  ~0U << 1,  ~0U << 2,  ~0U << 3,  ~0U << 4,  ~0U << 5,  ~0U << 6,
        ~0U << 7,  ~0U << 8,  ~0U << 9,  ~0U << 10, ~0U << 11, ~0U << 12, ~0U << 13,
        ~0U << 14, ~0U << 15, ~0U << 16, ~0U << 17, ~0U << 18, ~0U << 19, ~0U << 20,
        ~0U << 21, ~0U << 22, ~0U << 23, ~0U << 24, ~0U << 25, ~0U << 26, ~0U << 27,
        ~0U << 28, ~0U << 29, ~0U << 30, ~0U << 31, 0U };

    int32x4_t retMask =
        _mm_setr_epi32(maskLUT[shift.shift_32[0]], maskLUT[shift.shift_32[1]],
            maskLUT[shift.shift_32[2]], maskLUT[shift.shift_32[3]]);
    return retMask;
}

#define SIMD_LANE_NDX int32x4_t{ 0, 1, 2, 3 }

#endif

#if !defined(USE_NEON)
#define _mm128_set1_ps _mm_set1_ps
#define _mm128_setr_ps _mm_setr_ps

#define _mm128_add_ps _mm_add_ps
#define _mm128_sub_ps _mm_sub_ps
#define _mm128_mul_ps _mm_mul_ps
#define _mm128_div_ps _mm_div_ps

#define _mm128_fmadd_ps(a, b, c)  _mm_add_ps(_mm_mul_ps(a,b), c)

#define _mm128_xor_ps _mm_xor_ps

#define _mm128_movemask_ps _mm_movemask_ps

static inline __m128 _mm128_dp4_ps(const __m128 a, const __m128 b) {
    __m128 prod = _mm_mul_ps(a, b);
    __m128 dp = _mm_add_ps(prod, _mm_shuffle_ps(prod, prod, _MM_SHUFFLE(2, 3, 0, 1)));
    dp = _mm_add_ps(dp, _mm_shuffle_ps(dp, dp, _MM_SHUFFLE(0, 1, 2, 3)));
    return dp;
}
#else
typedef float32x4_t __m128;
#define _mm128_set1_ps vdupq_n_f32
#define _mm128_setr_ps(f1, f2, f3, f4) float32x4_t{ f1, f2, f3, f4 }
#endif
