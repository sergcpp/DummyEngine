#if defined(_M_X86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)

#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target ("sse4.1")
#endif

#include "SWculling_SSE2.c"

#ifdef __GNUC__
#pragma GCC pop_options
#endif

#endif
