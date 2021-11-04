#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("avx512f")
#pragma GCC target ("avx512bw")
#pragma GCC target ("avx512dq")
#pragma clang attribute push (__attribute__((target("avx512f,avx512bw,avx512dq"))), apply_to=function)
#endif

#if !defined(__ANDROID__) && (!defined(_MSC_VER) || _MSC_VER > 1916)
#include "SWculling_AVX512.c"
#endif

#ifdef __GNUC__
#pragma clang attribute pop
#pragma GCC pop_options
#endif
