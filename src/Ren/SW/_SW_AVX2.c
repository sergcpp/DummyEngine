#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("avx2")
#pragma GCC target ("fma")
#endif

#if !defined(__ANDROID__)
#include "SWculling_AVX2.c"
#endif

unsigned long long get_xcr_feature_mask() {
    return _xgetbv(0);
}

#ifdef __GNUC__
#pragma GCC pop_options
#endif