#if !defined(__aarch64__)

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("avx2")
#pragma GCC target ("fma")
#pragma clang attribute push (__attribute__((target("avx2,fma"))), apply_to=function)
#endif

#include "Utils_AVX.cpp"

#ifdef __GNUC__
#pragma clang attribute pop
#pragma GCC pop_options
#endif

#endif
