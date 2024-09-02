#pragma once

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <type_traits>

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline
#endif

namespace Gui {
enum eUninitialized { Uninitialize };

template <typename T> force_inline T Pi() { return T(3.1415926535897932384626433832795); }

template <typename T, int N> class Vec {
  protected:
    T data_[N];

  public:
    force_inline explicit Vec(eUninitialized) noexcept {}
    force_inline Vec() noexcept : data_{(T)0} {}
    force_inline Vec(const T v) noexcept {
        for (int i = 0; i < N; i++) {
            data_[i] = v;
        }
    }

    template <typename... Tail>
    force_inline explicit Vec(typename std::enable_if<sizeof...(Tail) + 1 == N, T>::type head, Tail... tail) noexcept
        : data_{head, T(tail)...} {}

    template <typename S, int M> force_inline explicit Vec(const Vec<S, M> &rhs) noexcept {
        const int count = N < M ? N : M;
        for (int i = 0; i < count; i++) {
            data_[i] = (T)rhs[i];
        }
        for (int i = count; i < N; i++) {
            data_[i] = T(0);
        }
    }

    template <typename S> force_inline explicit Vec(const Vec<S, N> &rhs) noexcept {
        for (int i = 0; i < N; i++) {
            data_[i] = (T)rhs[i];
        }
    }

    force_inline T &operator[](const int i) { return data_[i]; }
    force_inline const T &operator[](const int i) const { return data_[i]; }

    force_inline friend bool operator==(const Vec<T, N> &lhs, const Vec<T, N> &rhs) {
        for (int i = 0; i < N; i++) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }

    force_inline friend Vec<T, N> operator-(const Vec<T, N> &v) {
        auto res = Vec<T, N>{Uninitialize};
        for (int i = 0; i < N; i++) {
            res.data_[i] = -v.data_[i];
        }
        return res;
    }

    force_inline friend Vec<T, N> operator+(const Vec<T, N> &v) { return v; }

    force_inline Vec<T, N> &operator+=(const Vec<T, N> &rhs) {
        for (int i = 0; i < N; i++) {
            data_[i] += rhs.data_[i];
        }
        return *this;
    }
    force_inline Vec<T, N> &operator-=(const Vec<T, N> &rhs) {
        for (int i = 0; i < N; i++) {
            data_[i] -= rhs.data_[i];
        }
        return *this;
    }
    force_inline Vec<T, N> &operator*=(const Vec<T, N> &rhs) {
        for (int i = 0; i < N; i++) {
            data_[i] *= rhs.data_[i];
        }
        return *this;
    }
    force_inline Vec<T, N> &operator/=(const Vec<T, N> &rhs) {
        for (int i = 0; i < N; i++) {
            data_[i] /= rhs.data_[i];
        }
        return *this;
    }

#define DEFINE_ARITHMETIC_OPERATOR(OP)                                                                                 \
    force_inline friend Vec<T, N> operator OP(const Vec<T, N> &lhs, const Vec<T, N> &rhs) {                            \
        auto res = Vec<T, N>{Uninitialize};                                                                            \
        for (int i = 0; i < N; i++) {                                                                                  \
            res.data_[i] = lhs.data_[i] OP rhs.data_[i];                                                               \
        }                                                                                                              \
        return res;                                                                                                    \
    }

    DEFINE_ARITHMETIC_OPERATOR(+)
    DEFINE_ARITHMETIC_OPERATOR(-)
    DEFINE_ARITHMETIC_OPERATOR(*)
    DEFINE_ARITHMETIC_OPERATOR(/)
    DEFINE_ARITHMETIC_OPERATOR(%)

#undef DEFINE_ARITHMETIC_OPERATOR
};

template <typename T, int N> force_inline bool operator!=(const Vec<T, N> &lhs, const Vec<T, N> &rhs) {
    return !operator==(lhs, rhs);
}

template <typename T, int N> force_inline T Dot(const Vec<T, N> &lhs, const Vec<T, N> &rhs) {
    T res = lhs[0] * rhs[0];
    for (int i = 1; i < N; i++) {
        res += lhs[i] * rhs[i];
    }
    return res;
}

template <typename T> force_inline Vec<T, 3> Cross(const Vec<T, 3> &lhs, const Vec<T, 3> &rhs) {
    return Vec<T, 3>{lhs[1] * rhs[2] - lhs[2] * rhs[1], lhs[2] * rhs[0] - lhs[0] * rhs[2],
                     lhs[0] * rhs[1] - lhs[1] * rhs[0]};
}

template <typename T, int N> force_inline T Length(const Vec<T, N> &v) { return std::sqrt(Dot(v, v)); }

template <typename T, int N> force_inline T Length2(const Vec<T, N> &v) { return Dot(v, v); }

template <typename T, int N> force_inline T Distance(const Vec<T, N> &lhs, const Vec<T, N> &rhs) {
    Vec<T, N> temp = lhs - rhs;
    return std::sqrt(Dot(temp, temp));
}

template <typename T, int N> force_inline T Distance2(const Vec<T, N> &lhs, const Vec<T, N> &rhs) {
    Vec<T, N> temp = lhs - rhs;
    return Dot(temp, temp);
}

template <typename T, int N> force_inline Vec<T, N> Normalize(const Vec<T, N> &v) {
    T len = std::sqrt(Dot(v, v));
    return v / len;
}

template <typename T, int N> force_inline Vec<T, N> Min(const Vec<T, N> &v1, const Vec<T, N> &v2) {
    Vec<T, N> ret(Uninitialize);
    for (int i = 0; i < N; i++) {
        ret[i] = v1[i] < v2[i] ? v1[i] : v2[i];
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> Min(const Vec<T, N> &v1, const T v2) {
    Vec<T, N> ret(Uninitialize);
    for (int i = 0; i < N; i++) {
        ret[i] = v1[i] < v2 ? v1[i] : v2;
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> Min(const T v1, const Vec<T, N> &v2) {
    Vec<T, N> ret(Uninitialize);
    for (int i = 0; i < N; i++) {
        ret[i] = v1 < v2[i] ? v1 : v2[i];
    }
    return ret;
}

template <typename T> force_inline T Min(const T v1, const T v2) { return v1 < v2 ? v1 : v2; }

template <typename T, int N> force_inline Vec<T, N> Abs(const Vec<T, N> &v) {
    Vec<T, N> ret(Uninitialize);
    for (int i = 0; i < N; i++) {
        ret[i] = std::abs(v[i]);
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> Max(const Vec<T, N> &v1, const Vec<T, N> &v2) {
    Vec<T, N> ret(Uninitialize);
    for (int i = 0; i < N; i++) {
        ret[i] = v1[i] < v2[i] ? v2[i] : v1[i];
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> Max(const Vec<T, N> &v1, const T v2) {
    Vec<T, N> ret(Uninitialize);
    for (int i = 0; i < N; i++) {
        ret[i] = v1[i] < v2 ? v2 : v1[i];
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> Max(const T v1, const Vec<T, N> &v2) {
    Vec<T, N> ret(Uninitialize);
    for (int i = 0; i < N; i++) {
        ret[i] = v1 < v2[i] ? v2[i] : v1;
    }
    return ret;
}

template <typename T> force_inline T Max(const T v1, const T v2) { return v1 < v2 ? v2 : v1; }

template <typename T, typename U> force_inline T Clamp(const T &v1, const U min_val, const U max_val) {
    return Min(Max(v1, min_val), max_val);
}

template <typename T> force_inline T Saturate(const T &v1) { return Clamp(v1, T{0}, T{1}); }

template <typename T, typename S> force_inline T Mix(const T &x, const T &y, const S &a) {
    return x * (S(1) - a) + y * a;
}

template <typename T, int N> force_inline Vec<T, N> Floor(const Vec<T, N> &v) {
    Vec<T, N> ret;
    for (int i = 0; i < N; i++) {
        ret[i] = std::floor(v[i]);
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> AbsFloor(const Vec<T, N> &v) {
    Vec<T, N> ret;
    for (int i = 0; i < N; i++) {
        if (ret[i] > 0.0f) {
            ret[i] = std::floor(v[i]);
        } else {
            ret[i] = std::ceil(v[i]);
        }
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> Ceil(const Vec<T, N> &v) {
    Vec<T, N> ret;
    for (int i = 0; i < N; i++) {
        ret[i] = std::ceil(v[i]);
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> Fract(const Vec<T, N> &v) {
    Vec<T, N> ret;
    for (int i = 0; i < N; i++) {
        [[maybe_unused]] T integral_part;
        ret[i] = std::modf(v[i], &integral_part);
    }
    return ret;
}

template <typename T, int N> force_inline Vec<T, N> Mod(const Vec<T, N> &x, const Vec<T, N> &y) {
    return x - y * Floor(x / y);
}

template <typename T, int N> force_inline Vec<T, N> Step(const Vec<T, N> &x, const Vec<T, N> &edge) {
    Vec<T, N> ret;
    for (int i = 0; i < N; i++) {
        ret[i] = x[i] < edge[i] ? 0.0f : 1.0f;
    }
    return ret;
}

template <typename T, int N> force_inline const T *ValuePtr(const Vec<T, N> &v) { return &v[0]; }

template <typename T, int N> force_inline const T *ValuePtr(const Vec<T, N> *v) { return &(*v)[0]; }

template <typename T> force_inline Vec<T, 2> MakeVec2(const T *v) { return Vec<T, 2>(v[0], v[1]); }
template <typename T> force_inline Vec<T, 3> MakeVec3(const T *v) { return Vec<T, 3>(v[0], v[1], v[2]); }
template <typename T> force_inline Vec<T, 4> MakeVec4(const T *v) { return Vec<T, 4>(v[0], v[1], v[2], v[3]); }

using Vec2i = Vec<int32_t, 2>;
using Vec3i = Vec<int32_t, 3>;
using Vec4i = Vec<int32_t, 4>;

using Vec2u = Vec<uint32_t, 2>;
using Vec3u = Vec<uint32_t, 3>;
using Vec4u = Vec<uint32_t, 4>;

using Vec2f = Vec<float, 2>;
using Vec3f = Vec<float, 3>;
using Vec4f = Vec<float, 4>;

using Vec2d = Vec<double, 2>;
using Vec3d = Vec<double, 3>;
using Vec4d = Vec<double, 4>;
} // namespace Gui

#undef force_inline