
#if !defined(__ANDROID__) || defined(__i386__) || defined(__x86_64__)
extern "C" {
#include "SW/_SW_SSE2.c"
}

#include "Utils_SSE2.cpp"
#endif
