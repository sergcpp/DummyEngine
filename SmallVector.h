#pragma once

#include <cstdint>

#include <algorithm>
#include <new>

#if defined(_WIN32) || defined(__linux__)
#include <malloc.h>
#endif

#include <cassert>

namespace Sys {
template <class T, class U = T> T exchange(T &obj, U &&new_value) {
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}

#ifndef SYS_ALIGNED_MALLOC_DEFINED
inline void *aligned_malloc(size_t size, size_t alignment) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    return _mm_malloc(size, alignment);
#elif __STDC_VERSION__ >= 201112L
    return aligned_alloc(alignment, size);
#else
#ifdef __APPLE__
    void *p = nullptr;
    size_t mod = alignment % sizeof(void *);
    if (mod) {
        alignment += sizeof(void *) - mod;
    }
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
#define SYS_ALIGNED_MALLOC_DEFINED
#endif

template <typename T, int AlignmentOfT = alignof(T)> class SmallVectorImpl {
    T *begin_, *end_;
    size_t capacity_;

    // occupy one last bit of capacity to identify that we own the buffer
    static const size_t OwnerBit = (1ull << (8u * sizeof(size_t) - 1u));
    static const size_t CapacityMask = ~OwnerBit;

  protected:
    SmallVectorImpl(T *begin, T *end, const size_t capacity)
        : begin_(begin), end_(end), capacity_(capacity) {}

    ~SmallVectorImpl() {
        while (end_ != begin_) {
            (--end_)->~T();
        }

        if (capacity_ & OwnerBit) {
            aligned_free(begin_);
        }
    }

    SmallVectorImpl &operator=(const SmallVectorImpl &rhs) {
        if (&rhs == this) {
            return *this;
        }

        while (end_ != begin_) {
            (--end_)->~T();
        }

        if (capacity_ & OwnerBit) {
            aligned_free(begin_);
        }

        reserve(rhs.capacity_ & CapacityMask);

        end_ = begin_ + (rhs.end_ - rhs.begin_);

        if (rhs.end_ != rhs.begin_) {
            T *src = rhs.end_ - 1;
            T *dst = end_ - 1;
            do {
                new (dst--) T(*src--);
            } while (src >= rhs.begin_);
        }

        return (*this);
    }

    SmallVectorImpl &operator=(SmallVectorImpl &&rhs) noexcept {
        if (this == &rhs) {
            return (*this);
        }

        while (end_ != begin_) {
            (--end_)->~T();
        }

        if (capacity_ & OwnerBit) {
            aligned_free(begin_);
        }

        if (rhs.capacity_ & OwnerBit) {
            begin_ = exchange(rhs.begin_, nullptr);
            end_ = exchange(rhs.end_, nullptr);
            capacity_ = exchange(rhs.capacity_, 0);
        } else {
            reserve(rhs.capacity_ & CapacityMask);

            end_ = begin_ + (rhs.end_ - rhs.begin_);

            T *dst = end_ - 1;
            while (rhs.end_ != rhs.begin_) {
                new (dst--) T(std::move(*--rhs.end_));
                rhs.end_->~T();
            }
        }

        return (*this);
    }

  public:
    SmallVectorImpl(const SmallVectorImpl &rhs) = delete;
    SmallVectorImpl(SmallVectorImpl &&rhs) = delete;

    const T *cdata() const noexcept { return begin_; }
    const T *data() const noexcept { return begin_; }
    const T *begin() const noexcept { return begin_; }
    const T *end() const noexcept { return end_; }

    T *data() noexcept { return begin_; }
    T *begin() noexcept { return begin_; }
    T *end() noexcept { return end_; }

    const T &front() const {
        assert(begin_ != end_);
        return *begin_;
    }
    const T &back() const {
        assert(begin_ != end_);
        return *(end_ - 1);
    }

    T &front() {
        assert(begin_ != end_);
        return *begin_;
    }
    T &back() {
        assert(begin_ != end_);
        return *(end_ - 1);
    }

    bool empty() const noexcept { return end_ == begin_; }
    size_t size() const noexcept { return end_ - begin_; }
    size_t capacity() const noexcept { return (capacity_ & CapacityMask); }

    template <typename IntType> const T &operator[](const IntType i) const {
        assert(i >= 0 && begin_ + i < end_);
        return begin_[i];
    }

    template <typename IntType> T &operator[](const IntType i) {
        assert(i >= 0 && begin_ + i < end_);
        return begin_[i];
    }

    void push_back(const T &el) {
        reserve(size_t(end_ - begin_) + 1);
        new (end_++) T(el);
    }

    void push_back(T &&el) {
        reserve(size_t(end_ - begin_) + 1);
        new (end_++) T(std::move(el));
    }

    template <class... Args> T &emplace_back(Args &&...args) {
        reserve(size_t(end_ - begin_) + 1);
        new (end_++) T(std::forward<Args>(args)...);
        return *(end_ - 1);
    }

    void pop_back() {
        assert(begin_ != end_);
        (--end_)->~T();
    }

    void reserve(const size_t req_capacity) {
        const size_t cur_capacity = (capacity_ & CapacityMask);
        if (req_capacity <= cur_capacity) {
            return;
        }

        size_t new_capacity = cur_capacity;
        while (new_capacity < req_capacity) {
            new_capacity *= 2;
        }

        T *new_begin = (T *)aligned_malloc(new_capacity * sizeof(T), AlignmentOfT);
        T *new_end = new_begin + (end_ - begin_);

        if (end_ != begin_) {
            T *src = end_ - 1;
            T *dst = new_end - 1;
            do {
                new (dst--) T(std::move(*src));
                (src--)->~T();
            } while (src >= begin_);
        }

        if (capacity_ & OwnerBit) {
            aligned_free(begin_);
        }

        begin_ = new_begin;
        end_ = new_end;
        capacity_ = (new_capacity | OwnerBit);
    }

    void resize(const size_t req_size) {
        reserve(req_size);

        while (end_ > begin_ + req_size) {
            (--end_)->~T();
        }

        while (end_ < begin_ + req_size) {
            new (end_++) T();
        }
    }

    void clear() {
        for (T *el = end_; el > begin_;) {
            (--el)->~T();
        }
        end_ = begin_;
    }
};

template <typename T, int N, int AlignmentOfT = alignof(T)>
class SmallVector : public SmallVectorImpl<T, AlignmentOfT> {
    alignas(AlignmentOfT) char buffer_[sizeof(T) * N];

  public:
    SmallVector() : SmallVectorImpl<T>((T *)buffer_, (T *)buffer_, N) {} // NOLINT

    SmallVector(const SmallVector &rhs) // NOLINT
        : SmallVectorImpl<T>((T *)buffer_, (T *)buffer_, N) {
        SmallVectorImpl<T, AlignmentOfT>::operator=(rhs);
    }
    SmallVector(SmallVector &&rhs) : SmallVectorImpl<T>((T *)buffer_, (T *)buffer_, N) {  // NOLINT
        SmallVectorImpl<T, AlignmentOfT>::operator=(std::move(rhs));
    }

    SmallVector &operator=(const SmallVector &rhs) = delete;
    SmallVector &operator=(SmallVector &&rhs) noexcept {
        SmallVectorImpl<T, AlignmentOfT>::operator=(std::move(rhs));
        return (*this);
    }

    bool is_on_heap() const { return uintptr_t(this->begin()) != uintptr_t(&buffer_[0]); }
};
} // namespace Ren