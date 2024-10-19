#pragma once

#include "MVec.h"

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline
#endif

namespace Ren {
template <typename T, int M, int N> class Mat : public Vec<Vec<T, N>, M> {
  public:
    force_inline explicit Mat(eUninitialized) noexcept {}
    force_inline Mat() noexcept : Mat(T(1)) {}
    force_inline explicit Mat(T v) noexcept {
        for (int i = 0; i < M; ++i) {
            this->data_[i][i] = v;
        }
    }

    force_inline explicit Mat(const Mat<T, M - 1, N - 1> &v) noexcept {
        for (int i = 0; i < M - 1; ++i) {
            for (int j = 0; j < N - 1; ++j) {
                this->data_[i][j] = v[i][j];
            }
        }
        this->data_[M - 1][N - 1] = T(1);
    }

    force_inline explicit Mat(const Mat<T, M + 1, N + 1> &v) noexcept {
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j < N; ++j) {
                this->data_[i][j] = v[i][j];
            }
        }
    }

    template <typename... Tail>
    force_inline explicit Mat(typename std::enable_if<sizeof...(Tail) + 1 == M, Vec<T, N>>::type head,
                              Tail... tail) noexcept
        : Vec<Vec<T, N>, M>{head, tail...} {}

    force_inline Vec<T, N> &operator[](const int i) { return this->data_[i]; }
    force_inline const Vec<T, N> &operator[](const int i) const { return this->data_[i]; }

    force_inline friend bool operator==(const Mat<T, M, N> &lhs, const Mat<T, M, N> &rhs) {
        for (int i = 0; i < M; ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }

    force_inline Mat<T, M, N> &operator+=(const Mat<T, M, N> &rhs) {
        for (int i = 0; i < M; ++i) {
            this->data_[i] += rhs.data_[i];
        }
        return *this;
    }

    force_inline Mat<T, M, N> &operator-=(const Mat<T, M, N> &rhs) {
        for (int i = 0; i < M; ++i) {
            this->data_[i] -= rhs.data_[i];
        }
        return *this;
    }

    force_inline Mat<T, M, N> &operator*=(const Mat<T, M, N> &rhs) {
        (*this) = (*this) * rhs;
        return *this;
    }

    force_inline Mat<T, M, N> &operator/=(const Mat<T, M, N> &rhs) {
        for (int i = 0; i < M; ++i) {
            this->data_[i] /= rhs.data_[i];
        }
        return *this;
    }

    force_inline Mat<T, M, N> &operator*=(const T rhs) {
        for (int i = 0; i < M; ++i) {
            this->data_[i] *= rhs;
        }
        return *this;
    }

    force_inline Mat<T, M, N> &operator/=(const T rhs) {
        for (int i = 0; i < M; ++i) {
            this->data_[i] /= rhs;
        }
        return *this;
    }

    force_inline friend Mat<T, M, N> operator-(const Mat<T, M, N> &v) {
        auto res = Mat<T, M, N>{Uninitialize};
        for (int i = 0; i < M; ++i) {
            res.data_[i] = -v.data_[i];
        }
        return res;
    }

    force_inline friend Mat<T, M, N> operator+(const Mat<T, M, N> &lhs, const Mat<T, M, N> &rhs) {
        auto res = Mat<T, M, N>{Uninitialize};
        for (int i = 0; i < M; ++i) {
            res.data_[i] = lhs.data_[i] + rhs.data_[i];
        }
        return res;
    }

    force_inline friend Mat<T, M, N> operator-(const Mat<T, M, N> &lhs, const Mat<T, M, N> &rhs) {
        auto res = Mat<T, M, N>{Uninitialize};
        for (int i = 0; i < M; ++i) {
            res.data_[i] = lhs.data_[i] - rhs.data_[i];
        }
        return res;
    }

    force_inline friend Mat<T, M, N> operator/(const Mat<T, M, N> &lhs, const Mat<T, M, N> &rhs) {
        auto res = Mat<T, M, N>{Uninitialize};
        for (int i = 0; i < M; ++i) {
            res.data_[i] = lhs.data_[i] / rhs.data_[i];
        }
        return res;
    }

    force_inline friend Mat<T, M, N> operator*(const T lhs, const Mat<T, M, N> &rhs) {
        auto res = Mat<T, M, N>{Uninitialize};
        for (int i = 0; i < M; ++i) {
            res.data_[i] = lhs * rhs.data_[i];
        }
        return res;
    }

    force_inline friend Mat<T, M, N> operator/(const T lhs, const Mat<T, M, N> &rhs) {
        auto res = Mat<T, M, N>{Uninitialize};
        for (int i = 0; i < M; ++i) {
            res.data_[i] = lhs / rhs.data_[i];
        }
        return res;
    }

    force_inline friend Mat<T, M, N> operator*(const Mat<T, M, N> &lhs, const T &rhs) {
        auto res = Mat<T, M, N>{Uninitialize};
        for (int i = 0; i < M; ++i) {
            res.data_[i] = lhs.data_[i] * rhs;
        }
        return res;
    }

    force_inline friend Mat<T, M, N> operator/(const Mat<T, M, N> &lhs, const T &rhs) {
        auto res = Mat<T, M, N>{Uninitialize};
        for (int i = 0; i < M; ++i) {
            res.data_[i] = lhs.data_[i] / rhs;
        }
        return res;
    }
};

using Mat2i = Mat<int, 2, 2>;
using Mat2f = Mat<float, 2, 2>;
using Mat2d = Mat<double, 2, 2>;
using Mat3i = Mat<int, 3, 3>;
using Mat3f = Mat<float, 3, 3>;
using Mat3d = Mat<double, 3, 3>;
using Mat4i = Mat<int, 4, 4>;
using Mat4f = Mat<float, 4, 4>;
using Mat4d = Mat<double, 4, 4>;
using Mat4x3f = Mat<float, 4, 3>;
using Mat4x3d = Mat<double, 4, 3>;
using Mat3x4f = Mat<float, 3, 4>;
using Mat3x4d = Mat<double, 3, 4>;

template <typename T, int M, int N> Vec<T, M> operator*(const Vec<T, M> &lhs, const Mat<T, M, N> &rhs) {
    auto res = Vec<T, M>{Uninitialize};
    for (int m = 0; m < M; ++m) {
        res[m] = Dot(lhs, rhs[m]);
    }
    return res;
}

template <typename T, int M, int N> Vec<T, M> operator*(const Mat<T, M, N> &lhs, const Vec<T, M> &rhs) {
    auto res = Vec<T, M>{Uninitialize};
    for (int n = 0; n < N; ++n) {
        T sum = T(0);
        for (int m = 0; m < M; ++m) {
            sum += lhs[m][n] * rhs[m];
        }
        res[n] = sum;
    }
    return res;
}

template <typename T, int M, int N, int P> Mat<T, M, P> operator*(const Mat<T, M, N> &lhs, const Mat<T, N, P> &rhs) {
    auto res = Mat<T, M, P>{Uninitialize};
    for (int m = 0; m < M; ++m) {
        for (int p = 0; p < P; ++p) {
            T sum = T(0);
            for (int n = 0; n < N; ++n) {
                sum += rhs[m][n] * lhs[n][p];
            }
            res[m][p] = sum;
        }
    }
    return res;
}

template <typename T, int M, int N> force_inline Mat<T, N, M> Transpose(const Mat<T, M, N> &mat) {
    auto res = Mat<T, N, M>{Uninitialize};
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            res[n][m] = mat[m][n];
        }
    }
    return res;
}

template <typename T, int N> force_inline T Det(const Mat<T, N, N> &mat);

template <typename T, int N> T Minor(const Mat<T, N, N> &mat, const int row, const int col) {
    auto res = Mat<T, N - 1, N - 1>{Uninitialize};
    int dst_row = 0, dst_col;
    for (int src_row = 0; src_row < N; ++src_row) {
        if (src_row == row) {
            continue;
        }
        dst_col = 0;
        for (int src_col = 0; src_col < N; ++src_col) {
            if (src_col == col) {
                continue;
            }
            res[dst_row][dst_col] = mat[src_row][src_col];
            ++dst_col;
        }
        ++dst_row;
    }
    return Det(res);
}

template <typename T, int N> force_inline T Det(const Mat<T, N, N> &mat) {
    T sum = T(0);
    for (int n = 0; n < N; ++n) {
        const T minor = Minor(mat, n, 0);
        const T cofactor = (n & 1u) ? -minor : minor;
        sum += mat[n][0] * cofactor;
    }
    return sum;
}

template <typename T> force_inline T Det(const Mat<T, 1, 1> &mat) { return mat[0][0]; }

template <typename T> force_inline T Det(const Mat<T, 2, 2> &mat) {
    return mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];
}

template <typename T> force_inline T Det(const Mat<T, 3, 3> &mat) {
    return mat[0][0] * (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) -
           mat[1][0] * (mat[0][1] * mat[2][2] - mat[2][1] * mat[0][2]) +
           mat[2][0] * (mat[0][1] * mat[1][2] - mat[1][1] * mat[0][2]);
}

template <typename T> T Det(const Mat<T, 4, 4> &mat) {
    const T r0r1 = mat[0][2] * mat[1][3] - mat[1][2] * mat[0][3];
    const T r0r2 = mat[0][2] * mat[2][3] - mat[2][2] * mat[0][3];
    const T r0r3 = mat[0][2] * mat[3][3] - mat[3][2] * mat[0][3];
    const T r1r2 = mat[1][2] * mat[2][3] - mat[2][2] * mat[1][3];
    const T r1r3 = mat[1][2] * mat[3][3] - mat[3][2] * mat[1][3];
    const T r2r3 = mat[2][2] * mat[3][3] - mat[3][2] * mat[2][3];

    const T minor0 = mat[1][1] * r2r3 - mat[2][1] * r1r3 + mat[3][1] * r1r2;
    const T minor1 = mat[0][1] * r2r3 - mat[2][1] * r0r3 + mat[3][1] * r0r2;
    const T minor2 = mat[0][1] * r1r3 - mat[1][1] * r0r3 + mat[3][1] * r0r1;
    const T minor3 = mat[0][1] * r1r2 - mat[1][1] * r0r2 + mat[2][1] * r0r1;

    return mat[0][0] * minor0 - mat[1][0] * minor1 + mat[2][0] * minor2 - mat[3][0] * minor3;
}

template <typename T, int N> T Cofactor(const Mat<T, N, N> &mat, const int i, const int j) {
    const T minor = Minor(mat, i, j);
    return ((i + j) & 1u) ? -minor : minor;
}

template <typename T, int N> Mat<T, N, N> Adj(const Mat<T, N, N> &mat) {
    Mat<T, N, N> res = {Uninitialize};
    for (int row = 0; row < N; ++row) {
        for (int col = 0; col < N; ++col) {
            const T minor = Minor(mat, row, col);
            const T cofactor = ((row + col) & 1u) ? -minor : minor;
            res[col][row] = cofactor;
        }
    }
    return res;
}

template <typename T, int N> force_inline Mat<T, N, N> Inverse(const Mat<T, N, N> &mat) {
    const T det = Det(mat);
    return (T(1) / det) * Adj(mat);
}

template <typename T> Mat<T, 2, 2> Inverse(const Mat<T, 2, 2> &mat) {
    const T det = Det(mat);
    const T inv_det = T(1) / det;
    auto res = Mat<T, 2, 2>{Uninitialize};
    res[0][0] = inv_det * mat[1][1];
    res[0][1] = inv_det * -mat[0][1];
    res[1][0] = inv_det * -mat[1][0];
    res[1][1] = inv_det * mat[0][0];
    return res;
}

template <typename T> Mat<T, 3, 3> Inverse(const Mat<T, 3, 3> &mat) {
    const T minor0 = mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2];
    const T minor1 = mat[0][1] * mat[2][2] - mat[2][1] * mat[0][2];
    const T minor2 = mat[0][1] * mat[1][2] - mat[1][1] * mat[0][2];

    const T det = mat[0][0] * minor0 - mat[1][0] * minor1 + mat[2][0] * minor2;
    const T inv_det = T(1) / det;

    auto res = Mat<T, 3, 3>{Uninitialize};
    res[0][0] = inv_det * minor0;
    res[0][1] = inv_det * -minor1;
    res[0][2] = inv_det * minor2;
    res[1][0] = inv_det * (mat[2][0] * mat[1][2] - mat[1][0] * mat[2][2]);
    res[1][1] = inv_det * (mat[0][0] * mat[2][2] - mat[2][0] * mat[0][2]);
    res[1][2] = inv_det * (mat[1][0] * mat[0][2] - mat[0][0] * mat[1][2]);
    res[2][0] = inv_det * (mat[1][0] * mat[2][1] - mat[2][0] * mat[1][1]);
    res[2][1] = inv_det * (mat[2][0] * mat[0][1] - mat[0][0] * mat[2][1]);
    res[2][2] = inv_det * (mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1]);
    return res;
}

template <typename T> Mat<T, 4, 4> Inverse(const Mat<T, 4, 4> &mat) {
    T r0r1 = mat[0][2] * mat[1][3] - mat[1][2] * mat[0][3];
    T r0r2 = mat[0][2] * mat[2][3] - mat[2][2] * mat[0][3];
    T r0r3 = mat[0][2] * mat[3][3] - mat[3][2] * mat[0][3];
    T r1r2 = mat[1][2] * mat[2][3] - mat[2][2] * mat[1][3];
    T r1r3 = mat[1][2] * mat[3][3] - mat[3][2] * mat[1][3];
    T r2r3 = mat[2][2] * mat[3][3] - mat[3][2] * mat[2][3];

    T minor0 = mat[1][1] * r2r3 - mat[2][1] * r1r3 + mat[3][1] * r1r2;
    T minor1 = mat[0][1] * r2r3 - mat[2][1] * r0r3 + mat[3][1] * r0r2;
    T minor2 = mat[0][1] * r1r3 - mat[1][1] * r0r3 + mat[3][1] * r0r1;
    T minor3 = mat[0][1] * r1r2 - mat[1][1] * r0r2 + mat[2][1] * r0r1;
    const T det = mat[0][0] * minor0 - mat[1][0] * minor1 + mat[2][0] * minor2 - mat[3][0] * minor3;
    const T inv_det = T(1) / det;

    auto res = Mat<T, 4, 4>{Uninitialize};
    res[0][0] = inv_det * minor0;
    res[0][1] = inv_det * -minor1;
    res[0][2] = inv_det * minor2;
    res[0][3] = inv_det * -minor3;

    minor0 = mat[1][0] * r2r3 - mat[2][0] * r1r3 + mat[3][0] * r1r2;
    minor1 = mat[0][0] * r2r3 - mat[2][0] * r0r3 + mat[3][0] * r0r2;
    minor2 = mat[0][0] * r1r3 - mat[1][0] * r0r3 + mat[3][0] * r0r1;
    minor3 = mat[0][0] * r1r2 - mat[1][0] * r0r2 + mat[2][0] * r0r1;
    res[1][0] = inv_det * -minor0;
    res[1][1] = inv_det * minor1;
    res[1][2] = inv_det * -minor2;
    res[1][3] = inv_det * minor3;

    r0r1 = mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];
    r0r2 = mat[0][0] * mat[2][1] - mat[2][0] * mat[0][1];
    r0r3 = mat[0][0] * mat[3][1] - mat[3][0] * mat[0][1];
    r1r2 = mat[1][0] * mat[2][1] - mat[2][0] * mat[1][1];
    r1r3 = mat[1][0] * mat[3][1] - mat[3][0] * mat[1][1];
    r2r3 = mat[2][0] * mat[3][1] - mat[3][0] * mat[2][1];

    minor0 = mat[1][3] * r2r3 - mat[2][3] * r1r3 + mat[3][3] * r1r2;
    minor1 = mat[0][3] * r2r3 - mat[2][3] * r0r3 + mat[3][3] * r0r2;
    minor2 = mat[0][3] * r1r3 - mat[1][3] * r0r3 + mat[3][3] * r0r1;
    minor3 = mat[0][3] * r1r2 - mat[1][3] * r0r2 + mat[2][3] * r0r1;
    res[2][0] = inv_det * minor0;
    res[2][1] = inv_det * -minor1;
    res[2][2] = inv_det * minor2;
    res[2][3] = inv_det * -minor3;

    minor0 = mat[1][2] * r2r3 - mat[2][2] * r1r3 + mat[3][2] * r1r2;
    minor1 = mat[0][2] * r2r3 - mat[2][2] * r0r3 + mat[3][2] * r0r2;
    minor2 = mat[0][2] * r1r3 - mat[1][2] * r0r3 + mat[3][2] * r0r1;
    minor3 = mat[0][2] * r1r2 - mat[1][2] * r0r2 + mat[2][2] * r0r1;
    res[3][0] = inv_det * -minor0;
    res[3][1] = inv_det * minor1;
    res[3][2] = inv_det * -minor2;
    res[3][3] = inv_det * minor3;

    return res;
}

template <typename T> Mat<T, 4, 4> InverseAffine(const Mat<T, 4, 4> &mat) {
    const Mat<T, 3, 3> rot = Inverse(Mat<T, 3, 3>(mat));
    const Vec<T, 3> tr = Vec<T, 3>(mat[0][3], mat[1][3], mat[2][3]) * rot;
    return Mat<T, 4, 4>{Vec<T, 4>{rot[0], -tr[0]}, //
                        Vec<T, 4>{rot[1], -tr[1]}, //
                        Vec<T, 4>{rot[2], -tr[2]}, //
                        Vec<T, 4>{T(0), T(0), T(0), T(1)}};
}

template <typename T> force_inline Mat<T, 4, 4> Translate(const Mat<T, 4, 4> &m, const Vec<T, 3> &v) {
    Mat<T, 4, 4> res = m;
    res[3] += m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
    return res;
}

template <typename T> Mat<T, 4, 4> Rotate(const Mat<T, 4, 4> &m, T angle_rad, const Vec<T, 3> &_axis) {
    const T a = angle_rad;
    const T c = std::cos(a);
    const T s = std::sin(a);

    const Vec<T, 3> axis = Normalize(_axis);
    const Vec<T, 3> temp = (T(1) - c) * axis;

    Mat<T, 4, 4> rot(Uninitialize);
    rot[0][0] = c + temp[0] * axis[0];
    rot[0][1] = 0 + temp[0] * axis[1] + s * axis[2];
    rot[0][2] = 0 + temp[0] * axis[2] - s * axis[1];

    rot[1][0] = 0 + temp[1] * axis[0] - s * axis[2];
    rot[1][1] = c + temp[1] * axis[1];
    rot[1][2] = 0 + temp[1] * axis[2] + s * axis[0];

    rot[2][0] = 0 + temp[2] * axis[0] + s * axis[1];
    rot[2][1] = 0 + temp[2] * axis[1] - s * axis[0];
    rot[2][2] = c + temp[2] * axis[2];

    Mat<T, 4, 4> res(Uninitialize);
    res[0] = m[0] * rot[0][0] + m[1] * rot[0][1] + m[2] * rot[0][2];
    res[1] = m[0] * rot[1][0] + m[1] * rot[1][1] + m[2] * rot[1][2];
    res[2] = m[0] * rot[2][0] + m[1] * rot[2][1] + m[2] * rot[2][2];
    res[3] = m[3];

    return res;
}

template <typename T> force_inline Mat<T, 4, 4> Scale(const Mat<T, 4, 4> &m, const Vec<T, 3> &v) {
    return Mat<T, 4, 4>{m[0] * v[0], m[1] * v[1], m[2] * v[2], m[3]};
}

template <typename T, int M, int N> force_inline const T *ValuePtr(const Mat<T, M, N> &v) { return &v[0][0]; }

template <typename T, int M, int N> force_inline const T *ValuePtr(const Mat<T, M, N> *v) { return &(*v)[0][0]; }

template <typename T> void LookAt(Mat<T, 4, 4> &m, const Vec<T, 3> &src, const Vec<T, 3> &trg, const Vec<T, 3> &up) {
    const Vec<T, 3> f = Normalize(trg - src);
    const Vec<T, 3> s = Normalize(Cross(f, up));
    const Vec<T, 3> u = Cross(s, f);

    m[0][0] = s[0];
    m[0][1] = u[0];
    m[0][2] = -f[0];
    m[0][3] = T(0);
    m[1][0] = s[1];
    m[1][1] = u[1];
    m[1][2] = -f[1];
    m[1][3] = T(0);
    m[2][0] = s[2];
    m[2][1] = u[2];
    m[2][2] = -f[2];
    m[2][3] = T(0);
    m[3][0] = T(0);
    m[3][1] = T(0);
    m[3][2] = T(0);
    m[3][3] = T(1);

    m = Translate(m, -src);
}

template <typename T>
Mat<T, 4, 4> PerspectiveProjection(const T fov, const T aspect, const T znear, const T zfar,
                                   const bool z_range_zero_to_one) {
    const T xymax = znear * std::tan(fov * Pi<T>() / T(360));
    const T ymin = -xymax;
    const T xmin = -xymax;

    const T width = xymax - xmin;
    const T height = xymax - ymin;

    const T w = 2 * znear / (width * aspect);
    const T h = 2 * znear / height;

    Mat<T, 4, 4> m;

    m[0][0] = w;
    m[0][1] = m[0][2] = m[0][3] = T(0);

    m[1][1] = h;
    m[1][0] = m[1][2] = m[1][3] = T(0);

    m[2][0] = m[2][1] = T(0);
    if (z_range_zero_to_one) {
        m[2][2] = zfar / (znear - zfar);
    } else {
        m[2][2] = -(zfar + znear) / (zfar - znear);
    }
    m[2][3] = T(-1);

    m[3][0] = m[3][1] = T(0);
    if (z_range_zero_to_one) {
        m[3][2] = -(zfar * znear) / (zfar - znear);
    } else {
        m[3][2] = -2 * (zfar * znear) / (zfar - znear);
    }
    m[3][3] = T(0);

    return m;
}

template <typename T>
Mat<T, 4, 4> OrthographicProjection(const T left, const T right, const T bottom, const T top, const T nnear,
                                    const T ffar, const bool z_range_zero_to_one) {
    const T r_width = T(1) / (right - left);
    const T r_height = T(1) / (top - bottom);
    const T r_depth = T(1) / (ffar - nnear);

    Mat<T, 4, 4> m = Mat<T, 4, 4>{T(0)};

    m[0][0] = T(2) * r_width;
    m[1][1] = T(2) * r_height;
    if (z_range_zero_to_one) {
        m[2][2] = -r_depth;
    } else {
        m[2][2] = -T(2) * r_depth;
    }
    m[3][0] = -(right + left) * r_width;
    m[3][1] = -(top + bottom) * r_height;
    if (z_range_zero_to_one) {
        m[3][2] = -nnear * r_depth;
    } else {
        m[3][2] = -(ffar + nnear) * r_depth;
    }
    m[3][3] = T(1);

    return m;
}
} // namespace Ren

#undef force_inline