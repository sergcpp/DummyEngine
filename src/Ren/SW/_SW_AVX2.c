#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("avx2")
#pragma GCC target ("fma")
#pragma clang attribute push (__attribute__((target("avx2,fma"))), apply_to=function)
#endif

#if !defined(__ANDROID__)
#include "SWculling_AVX2.c"
#endif

unsigned long long get_xcr_feature_mask() {
    return _xgetbv(0);
}

#ifdef __GNUC__
#pragma clang attribute pop
#pragma GCC pop_options
#endif
