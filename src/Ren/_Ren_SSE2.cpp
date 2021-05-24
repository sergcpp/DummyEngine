#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("sse4.1")
#pragma GCC target ("sse2")
#endif

#if !defined(__ANDROID__) || defined(__i386__) || defined(__x86_64__)
#include "Utils_SSE2.cpp"
#endif

#ifdef __GNUC__
#pragma GCC pop_options
#endif