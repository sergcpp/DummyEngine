#pragma once

#include <cstdint>
#include <cstring>

namespace Net {
struct Hash {
    uint32_t hash;
};

#if defined(_MSC_VER)
#if _MSC_VER <= 1800
#define CONSTEXPR inline
#else
#define CONSTEXPR constexpr
#endif
#else
#define CONSTEXPR constexpr
#endif

template <const uint32_t N, const uint32_t I = 0> struct hash_calc {
    static CONSTEXPR uint32_t apply(const char (&s)[N]) { return (hash_calc<N, I + 1>::apply(s) ^ s[I]) * 16777619u; };
};

template <const uint32_t N> struct hash_calc<N, N> {
    static CONSTEXPR uint32_t apply(const char (&s)[N]) { return 2166136261u; };
};

template <uint32_t N> CONSTEXPR Hash static_hash(const char (&s)[N]) { return Hash{hash_calc<N>::apply(s)}; }

template <class T, typename E = void> class Var;

template <class T> class Var<T, typename std::enable_if<!std::is_fundamental<T>::value>::type> : public T {
    friend class VarContainer;

    Hash hash_;

  public:
    template <uint32_t N> inline Var(const char (&s)[N]) : hash_(static_hash(s)) {}

    template <uint32_t N> inline Var(const char (&s)[N], const T &var) : T(var), hash_(static_hash(s)) {}

    template <uint32_t N> inline Var(const char (&s)[N], T &&var) : T(var), hash_(static_hash(s)) {}
    // Var(Hash h) : hash_(h) {}

    uint32_t hash() const { return hash_.hash; }

    T &val() { return static_cast<T>(*this); }

    T *p_val() { return this; }

    const T &val() const { return static_cast<const T>(*this); }

    const T *p_val() const { return this; }

    void set_hash(uint32_t h) { hash_.hash = h; }

    Var &operator=(T &var) {
        *static_cast<T *>(this) = var;
        return *this;
    }

    Var &operator=(T &&var) {
        *static_cast<T *>(this) = var;
        return *this;
    }
};

template <class T> class Var<T, typename std::enable_if<std::is_fundamental<T>::value>::type> {
    friend class VarContainer;

    Hash hash_;
    T var_;

  public:
    template <uint32_t N> inline Var(const char (&s)[N]) : hash_(static_hash(s)) {}

    template <uint32_t N> inline Var(const char (&s)[N], T &var) : hash_(static_hash(s)), var_(var) {}

    template <uint32_t N> inline Var(const char (&s)[N], T &&var) : hash_(static_hash(s)), var_(var) {}

    /*explicit Var(Hash h) : hash_(h) {}
    explicit Var(Hash h, T &var) : hash_(h), var_(var) {}
    explicit Var(Hash h, T &&var) : hash_(h), var_(var) {}*/

    operator T() { return var_; }

    T &val() { return var_; }

    T *p_val() { return &var_; }

    const T &val() const { return var_; }

    const T *p_val() const { return &var_; }

    void set_val(const T &v) { var_ = v; }

    uint32_t hash() const { return hash_.hash; }

    void set_hash(uint32_t h) { hash_.hash = h; }

    Var &operator=(T &var) {
        var_ = var;
        return *this;
    }

    Var &operator=(T &&var) {
        var_ = var;
        return *this;
    }
};
} // namespace Net
