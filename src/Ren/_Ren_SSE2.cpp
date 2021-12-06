#if defined(_M_X86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("sse4.1")
#pragma GCC target ("sse2")
#pragma clang attribute push (__attribute__((target("sse4.1,sse2"))), apply_to=function)
#endif

#include "Utils_SSE2.cpp"

#ifdef __GNUC__
#pragma clang attribute pop
#pragma GCC pop_options
#endif

#endif
