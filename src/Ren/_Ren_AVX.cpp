#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("avx2")
#pragma GCC target ("fma")
#endif

#if !defined(__ANDROID__)
#include "Utils_AVX.cpp"
#endif

#ifdef __GNUC__
#pragma GCC pop_options
#endif