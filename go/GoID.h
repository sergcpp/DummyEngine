#pragma once

#include <cstdint>

#if defined(_MSC_VER)
#if _MSC_VER <= 1800
#define CONSTEXPR inline
#else
#define CONSTEXPR constexpr
#endif
#else
#define CONSTEXPR constexpr
#endif

#pragma warning(disable : 4307)

template<const uint32_t N, const uint32_t I = 0>
struct hash_calc {
    static CONSTEXPR uint32_t apply(const char(&s)[N]) {
        return  (::hash_calc<N, I + 1>::apply(s) ^ s[I]) * 16777619u;
    };
};

template<const uint32_t N>
struct hash_calc<N, N> {
    static CONSTEXPR uint32_t apply(const char(&s)[N]) {
        return 2166136261u;
    };
};

template<uint32_t N>
CONSTEXPR uint32_t static_hash(const char(&s)[N]) {
    return { hash_calc<N>::apply(s) };
}

inline uint32_t non_static_hash(const char *s) {
    if (*s) {
        return (non_static_hash(s + 1) ^ *s) * 16777619u;
    } else {
        return 16777619u * 2166136261u;
    }
}

class GoID {
    uint32_t hash_;
public:
    explicit GoID(int k) : hash_((uint32_t)k) {}
    explicit GoID(unsigned int k) : hash_(k) {}
    template<uint32_t N>
    explicit GoID(const char (&k)[N]) : hash_(static_hash(k)) {}
    explicit GoID(const char *k) : hash_(non_static_hash(k)) {}

    uint32_t hash() const {
        return hash_;
    }

    bool operator==(const GoID &rhs) {
        return rhs.hash_ == hash_;
    }
};

#undef CONSTEXPR
