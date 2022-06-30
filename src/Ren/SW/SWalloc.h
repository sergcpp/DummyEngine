#ifndef SW_ALLOC_H
#define SW_ALLOC_H

#include <assert.h>
#include <stdint.h>

inline void* sw_aligned_malloc(size_t size, size_t alignment) {
    assert(alignment > sizeof(void *));
    size_t space = size + (alignment - 1);

    void *ptr = malloc(space + sizeof(void *));
    void *original_ptr = ptr;

    char *ptr_bytes = (char *)ptr;
    ptr_bytes += sizeof(void *);

    size_t off = (size_t)((uintptr_t)(ptr_bytes) % alignment);
    if (off) {
        off = alignment - off;
    }
    ptr_bytes += off;
    assert(((uintptr_t)(ptr_bytes) % alignment) == 0);

    ptr = ptr_bytes;
    ptr_bytes -= sizeof(void *);

    memcpy(ptr_bytes, &original_ptr, sizeof(void *));

    return ptr;
}

inline void sw_aligned_free(void* p) {
    if (p) {
        free(((void **)p)[-1]);
    }
}

#endif // SW_ALLOC_H