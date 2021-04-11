#pragma once

#include <cstdint>

#include <algorithm>
#include <new>

#if defined(_WIN32) || defined(__linux__)
#include <malloc.h>
#endif

#include <cassert>

namespace Sys {
inline void *aligned_malloc(size_t size, size_t alignment) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    return _mm_malloc(size, alignment);
#elif __STDC_VERSION__ >= 201112L
    return aligned_alloc(alignment, size);
#else
#ifdef __APPLE__
    void *p = nullptr;
    size_t mod = alignment % sizeof(void *);
    if (mod)
        alignment += sizeof(void *) - mod;
    posix_memalign(&p, alignment, size);
    return p;
#else
    return memalign(alignment, size);
#endif
#endif
}

inline void aligned_free(void *p) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    _mm_free(p);
#elif __STDC_VERSION__ >= 201112L
    free(p);
#else
    free(p);
#endif
}

template <typename T> class SmallVectorImpl {
    T *begin_, *end_;
    size_t capacity_;

    // occupy one last bit of capacity to identify that we own the buffer
    static const size_t OwnerBit = (1u << 0u);
    static const size_t CapacityMask = ~OwnerBit;

  protected:
    SmallVectorImpl(T *begin, T *end, const size_t capacity)
        : begin_(begin), end_(end), capacity_(capacity) {
        assert((capacity_ & OwnerBit) == 0 && "Capacity should be even!");
    }

    ~SmallVectorImpl() {
        if (capacity_ & OwnerBit) {
            aligned_free(begin_);
        } else {
            for (T *el = begin_; el != end_; el++) {
                el->~T();
            }
        }
    }

  public:
    const T *begin() const { return begin_; }
    const T *end() const { return end_; }

    const T &front() const { return *begin_; }
    const T &back() const { return *(end_ - 1); }

    T *begin() { return begin_; }
    T *end() { return end_; }

    T &front() { return *begin_; }
    T &back() { return *(end_ - 1); }

    bool empty() const { return end_ == begin_; }
    size_t size() const { return end_ - begin_; }
    size_t capacity() const { return (capacity_ & CapacityMask); }

    template <typename IntType> const T &operator[](const IntType i) const {
        return begin_[size_t(i)];
    }

    template <typename IntType> T &operator[](const IntType i) {
        return begin_[size_t(i)];
    }

    void push_back(const T &el) {
        reserve(size_t(end_ - begin_) + 1);
        (*end_++) = el;
    }

    void push_back(T &&el) {
        reserve(size_t(end_ - begin_) + 1);
        (*end_++) = std::move(el);
    }

    template <class... Args> void emplace_back(Args &&... args) {
        reserve(size_t(end_ - begin_) + 1);
        new (end_++) T(std::forward<Args>(args)...);
    }

    void pop_back() {
        --end_;
        end_->~T();
    }

    void reserve(const size_t req_capacity) {
        if (req_capacity <= (capacity_ & CapacityMask)) {
            return;
        }

        size_t new_capacity = (capacity_ & CapacityMask);
        while (new_capacity < req_capacity) {
            new_capacity *= 2;
        }

        T *new_begin = (T *)aligned_malloc(new_capacity * sizeof(T), alignof(T));
        T *new_end = new_begin + (end_ - begin_);

        T *src = end_ - 1, *dst = new_end - 1;
        do {
            (*dst) = std::move(*src);
            src->~T();

            --src;
            --dst;
        } while (src >= begin_);

        if (capacity_ & OwnerBit) {
            aligned_free(begin_);
        }

        begin_ = new_begin;
        end_ = new_end;
        capacity_ = (new_capacity | OwnerBit);
    }
};

template <typename T, int N, int SizeOfT = sizeof(T), int AlignmentOfT = alignof(T)>
class SmallVector : public SmallVectorImpl<T> {
    alignas(AlignmentOfT) char buffer_[SizeOfT * N];

  public:
    SmallVector() : SmallVectorImpl<T>((T *)buffer_, (T *)buffer_, N) {}

    bool is_on_heap() const { return uintptr_t(this->begin()) != uintptr_t(&buffer_[0]); }
};
} // namespace Sys