#ifndef SW_ALLOC_H
#define SW_ALLOC_H

#if defined(_WIN32) || defined(__linux__)
#include <malloc.h>
#endif

inline void* sw_aligned_malloc(size_t size, size_t alignment) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    return _mm_malloc(size, alignment);
#elif __STDC_VERSION__ >= 201112L
    return aligned_alloc(alignment, size);
#elif defined(__APPLE__)
    void* p = nullptr;
    size_t mod = alignment % sizeof(void*);
    if (mod) alignment += sizeof(void*) - mod;
    posix_memalign(&p, alignment, size);
    return p;
#else
    return memalign(alignment, size);
#endif
}

inline void sw_aligned_free(void* p) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    _mm_free(p);
#else
    free(p);
#endif
}

#endif // SW_ALLOC_H