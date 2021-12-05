#if defined(__i386__) || defined(__x86_64__)

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("sse4.1")
#endif

#include "SWculling_SSE2.c"

#ifdef __GNUC__
#pragma GCC pop_options
#endif

#endif
