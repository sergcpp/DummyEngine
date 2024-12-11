#pragma once

#include <cstddef>
#include <cstdint>

#include <type_traits>
#include <vector>

#include "SmallVector.h"

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline
#endif

namespace Phy {
template <typename T> struct remove_all_const : std::remove_const<T> {};

template <typename T> struct remove_all_const<T *> {
    typedef typename remove_all_const<T>::type *type;
};

template <typename T> struct remove_all_const<T *const> {
    typedef typename remove_all_const<T>::type *type;
};

template <typename T> class Span {
    T *p_data_ = nullptr;
    ptrdiff_t size_ = 0;

  public:
    Span() = default;
    Span(T *p_data, const ptrdiff_t size) : p_data_(p_data), size_(size) {}
    Span(T *p_data, const size_t size) : p_data_(p_data), size_(size) {}
#if INTPTR_MAX == INT64_MAX
    Span(T *p_data, const int size) : p_data_(p_data), size_(size) {}
    Span(T *p_data, const uint32_t size) : p_data_(p_data), size_(size) {}
#endif
    Span(T *p_begin, T *p_end) : p_data_(p_begin), size_(p_end - p_begin) {}
    template <typename Alloc>
    Span(const std::vector<typename remove_all_const<T>::type, Alloc> &v) : Span(v.data(), size_t(v.size())) {}
    template <typename Alloc>
    Span(std::vector<typename remove_all_const<T>::type, Alloc> &v) : Span(v.data(), size_t(v.size())) {}
    template <typename Alloc>
    Span(const SmallVectorImpl<typename remove_all_const<T>::type, Alloc> &v) : Span(v.data(), v.size()) {}
    template <typename Alloc>
    Span(SmallVectorImpl<typename remove_all_const<T>::type, Alloc> &v) : Span(v.data(), v.size()) {}

    template <size_t N> Span(T (&arr)[N]) : p_data_(arr), size_(N) {}

    template <typename U = typename std::remove_const<T>::type,
              typename = typename std::enable_if<!std::is_same<T, U>::value>::type>
    Span(const Span<U> &rhs) : Span(rhs.data(), rhs.size()) {}

    Span(const Span &rhs) = default;
    Span &operator=(const Span &rhs) = default;

    force_inline T *data() const { return p_data_; }
    force_inline ptrdiff_t size() const { return size_; }
    force_inline bool empty() const { return size_ == 0; }

    force_inline T &operator[](const ptrdiff_t i) const {
        assert(i >= 0 && i < size_);
        return p_data_[i];
    }
    force_inline T &operator()(const ptrdiff_t i) const {
        assert(i >= 0 && i < size_);
        return p_data_[i];
    }

    template <typename U>
    bool operator==(const Span<U> &rhs) const {
        if (size_ != rhs.size()) {
            return false;
        }
        bool eq = true;
        for (uint32_t i = 0; i < size_ && eq; ++i) {
            eq &= p_data_[i] == rhs[i];
        }
        return eq;
    }
    template <typename U>
    bool operator!=(const Span<U> &rhs) const {
        if (size_ != rhs.size()) {
            return true;
        }
        bool neq = false;
        for (uint32_t i = 0; i < size_ && !neq; ++i) {
            neq |= p_data_[i] != rhs[i];
        }
        return neq;
    }
    template <typename U>
    bool operator<(const Span<U> &rhs) const {
        return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
    }
    template <typename U>
    bool operator<=(const Span<U> &rhs) const {
        return !std::lexicographical_compare(rhs.begin(), rhs.end(), begin(), end());
    }
    template <typename U>
    bool operator>(const Span<U> &rhs) const {
        return std::lexicographical_compare(rhs.begin(), rhs.end(), begin(), end());
    }
    template <typename U>
    bool operator>=(const Span<U> &rhs) const {
        return !std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
    }

    using iterator = T *;
    using const_iterator = const T *;

    force_inline iterator begin() const { return p_data_; }
    force_inline iterator end() const { return p_data_ + size_; }
    force_inline const_iterator cbegin() const { return p_data_; }
    force_inline const_iterator cend() const { return p_data_ + size_; }
};
} // namespace Ren
