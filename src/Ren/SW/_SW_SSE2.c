#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("sse4.1")
#endif

#if !defined(__ANDROID__) || defined(__i386__) || defined(__x86_64__)
#include "SWculling_SSE2.c"
#endif

#ifdef __GNUC__
#pragma GCC pop_options
#endif