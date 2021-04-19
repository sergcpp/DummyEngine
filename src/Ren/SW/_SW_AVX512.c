#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("avx512f")
#pragma GCC target ("avx512bw")
#pragma GCC target ("avx512dq")
#endif

#if !defined(__ANDROID__)
#include "SWculling_AVX512.c"
#endif

#ifdef __GNUC__
#pragma GCC pop_options
#endif