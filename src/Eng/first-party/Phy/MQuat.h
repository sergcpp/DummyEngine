#pragma once

#include <limits>

#include "MVec.h"

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline
#endif

namespace Phy {
template <typename T> class QuatT {
  public:
    T x, y, z, w;

    force_inline explicit QuatT(eUninitialized) {}
    force_inline QuatT() : x(0), y(0), z(0), w(1) {}
    force_inline QuatT(const T _x, const T _y, const T _z, const T _w) : x(_x), y(_y), z(_z), w(_w) {}
    QuatT(Vec<T, 3> n, const T angle_rad) {
        const T half_angle_rad = T(0.5) * angle_rad;

        w = std::cos(half_angle_rad);

        const T half_sine = std::sin(half_angle_rad);

        n = Normalize(n);
        x = n[0] * half_sine;
        y = n[1] * half_sine;
        z = n[2] * half_sine;
    }

    force_inline T &operator[](const int i) {
        T *data = &x;
        return data[i];
    }
    force_inline const T &operator[](const int i) const {
        const T *data = &x;
        return data[i];
    }

    force_inline QuatT<T> &operator+=(const QuatT<T> &rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    }

    force_inline QuatT<T> &operator-=(const QuatT<T> &rhs) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    }

    force_inline QuatT<T> &operator*=(const T rhs) {
        x *= rhs;
        y *= rhs;
        z *= rhs;
        w *= rhs;
        return *this;
    }

    force_inline QuatT<T> &operator/=(const T rhs) {
        x /= rhs;
        y /= rhs;
        z /= rhs;
        w /= rhs;
        return *this;
    }

    force_inline friend QuatT<T> operator-(const QuatT<T> &v) {
        auto res = QuatT<T>{Uninitialize};

        res.x = -v.x;
        res.y = -v.y;
        res.z = -v.z;
        res.w = -v.w;

        return res;
    }

    force_inline friend QuatT<T> operator*(const T lhs, const QuatT<T> &rhs) {
        auto res = QuatT<T>{Uninitialize};

        res.x = lhs * rhs.x;
        res.y = lhs * rhs.y;
        res.z = lhs * rhs.z;
        res.w = lhs * rhs.w;

        return res;
    }

    force_inline friend QuatT<T> operator/(const T lhs, const QuatT<T> &rhs) {
        auto res = QuatT<T>{Uninitialize};

        res.x = lhs / rhs.x;
        res.y = lhs / rhs.y;
        res.z = lhs / rhs.z;
        res.w = lhs / rhs.w;

        return res;
    }

    force_inline friend QuatT<T> operator*(const QuatT<T> &lhs, const T rhs) {
        auto res = QuatT<T>{Uninitialize};

        res.x = lhs.x * rhs;
        res.y = lhs.y * rhs;
        res.z = lhs.z * rhs;
        res.w = lhs.w * rhs;

        return res;
    }

    force_inline friend QuatT<T> operator/(const QuatT<T> &lhs, const T rhs) {
        auto res = QuatT<T>{Uninitialize};

        res.x = lhs.x / rhs;
        res.y = lhs.y / rhs;
        res.z = lhs.z / rhs;
        res.w = lhs.w / rhs;

        return res;
    }

    force_inline friend QuatT<T> operator+(const QuatT<T> &lhs, const QuatT<T> &rhs) {
        auto res = QuatT<T>{Uninitialize};
        res.x = lhs.x + rhs.x;
        res.y = lhs.y + rhs.y;
        res.z = lhs.z + rhs.z;
        res.w = lhs.w + rhs.w;
        return res;
    }

    force_inline friend QuatT<T> operator-(const QuatT<T> &lhs, const QuatT<T> &rhs) {
        auto res = QuatT<T>{Uninitialize};
        res.x = lhs.x - rhs.x;
        res.y = lhs.y - rhs.y;
        res.z = lhs.z - rhs.z;
        res.w = lhs.w - rhs.w;
        return res;
    }

    force_inline friend QuatT<T> operator*(const QuatT<T> &lhs, const QuatT<T> &rhs) {
        auto res = QuatT<T>{Uninitialize};

        res.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
        res.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
        res.y = lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z;
        res.z = lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x;

        return res;
    }
};

template <typename T> force_inline T Dot(const QuatT<T> &lhs, const QuatT<T> &rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

template <typename T> force_inline QuatT<T> Inverse(const QuatT<T> &q) {
    auto res = q;

    res /= Dot(q, q);
    res.x = -res.x;
    res.y = -res.y;
    res.z = -res.z;

    return res;
}

template <typename T> force_inline T Roll(const QuatT<T> &q) {
    return std::atan2(T(2) * (q.x * q.y + q.w * q.z), q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z);
}

template <typename T> force_inline T Pitch(const QuatT<T> &q) {
    return std::atan2(T(2) * (q.y * q.z + q.w * q.x), q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z);
}

template <typename T> force_inline T Yaw(const QuatT<T> &q) {
    return std::asin(clamp(T(-2) * (q.x * q.z - q.w * q.y), T(-1), T(1)));
}

template <typename T> force_inline Vec<T, 3> EulerAngles(const QuatT<T> &q) {
    return Vec<T, 3>{Pitch(q), Yaw(q), Roll(q)};
}

template <typename T> force_inline QuatT<T> Normalize(const QuatT<T> &q) {
    auto res = QuatT<T>{Uninitialize};

    const T inv_len = T(1) / std::sqrt(Dot(q, q));
    res.x = q.x * inv_len;
    res.y = q.y * inv_len;
    res.z = q.z * inv_len;
    res.w = q.w * inv_len;

    return res;
}

template <typename T> QuatT<T> Slerp(const QuatT<T> &q0, const QuatT<T> &q1, const T a) {
    QuatT<T> q2 = q1;

    T cos_theta = Dot(q0, q1);

    if (cos_theta < 0) {
        q2 = -q1;
        cos_theta = -cos_theta;
    }

    if (cos_theta > 1 - std::numeric_limits<T>::epsilon()) {
        return QuatT<T>{Mix(q0.x, q1.x, a), Mix(q0.y, q1.y, a), Mix(q0.z, q1.z, a), Mix(q0.w, q1.w, a)};
    } else {
        const T angle = std::acos(cos_theta);
        return (std::sin((T(1) - a) * angle) * q0 + std::sin(a * angle) * q2) / std::sin(angle);
    }
}

template <typename T> Mat<T, 3, 3> ToMat3(const QuatT<T> &vec) {
    Mat<T, 3, 3> ret;

    const T qxx = vec[0] * vec[0];
    const T qyy = vec[1] * vec[1];
    const T qzz = vec[2] * vec[2];
    const T qxz = vec[0] * vec[2];
    const T qxy = vec[0] * vec[1];
    const T qyz = vec[1] * vec[2];
    const T qwx = vec[3] * vec[0];
    const T qwy = vec[3] * vec[1];
    const T qwz = vec[3] * vec[2];

    ret[0][0] = 1 - 2 * (qyy + qzz);
    ret[0][1] = 2 * (qxy + qwz);
    ret[0][2] = 2 * (qxz - qwy);

    ret[1][0] = 2 * (qxy - qwz);
    ret[1][1] = 1 - 2 * (qxx + qzz);
    ret[1][2] = 2 * (qyz + qwx);

    ret[2][0] = 2 * (qxz + qwy);
    ret[2][1] = 2 * (qyz - qwx);
    ret[2][2] = 1 - 2 * (qxx + qyy);

    return ret;
}

template <typename T> QuatT<T> ToQuat(const T yaw /* Z */, const T pitch /* Y */, const T roll /* X */) {
    const T cy = std::cos(yaw * T(0.5));
    const T sy = std::sin(yaw * T(0.5));
    const T cp = std::cos(pitch * T(0.5));
    const T sp = std::sin(pitch * T(0.5));
    const T cr = std::cos(roll * T(0.5));
    const T sr = std::sin(roll * T(0.5));

    QuatT<T> q;

    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;

    return q;
}

template <typename T> force_inline Mat<T, 4, 4> ToMat4(const QuatT<T> &vec) { return Mat<T, 4, 4>{ToMat3(vec)}; }

template <typename T> force_inline QuatT<T> MakeQuat(const T *v) { return QuatT<T>{v[0], v[1], v[2], v[3]}; }

using Quatf = QuatT<float>;
using Quatd = QuatT<double>;
} // namespace Phy
