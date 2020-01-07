#pragma once

#include "MVec.h"

namespace Ren {
template <typename T>
T Pi() {
    return T(3.1415926535897932384626433832795);
}

template <typename T, int M, int N>
class Mat : public Vec<Vec<T, N>, M> {
public:
    Mat(eUninitialized) noexcept {}
    Mat() noexcept : Mat(T(1)) {}
    explicit Mat(T v) noexcept {
        for (int i = 0; i < M; i++) {
            this->data_[i][i] = v;
        }
    }

    explicit Mat(const Mat<T, M - 1, N - 1> &v) noexcept {
        for (int i = 0; i < M - 1; i++) {
            for (int j = 0; j < N - 1; j++) {
                this->data_[i][j] = v[i][j];
            }
        }
        this->data_[M - 1][N - 1] = T(1);
    }

    explicit Mat(const Mat<T, M + 1, N + 1> &v) noexcept {
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < N; j++) {
                this->data_[i][j] = v[i][j];
            }
        }
    }

    template <typename... Tail>
    Mat(typename std::enable_if<sizeof...(Tail)+1 == M, Vec<T, N>>::type head, Tail... tail) noexcept
        : Vec<Vec<T, N>, M> { head, tail... } {
    }

    Vec<T, N> &operator[](int i) {
        return this->data_[i];
    }
    const Vec<T, N> &operator[](int i) const {
        return this->data_[i];
    }

    friend bool operator==(const Mat<T, M, N> &lhs, const Mat<T, M, N> &rhs) {
        bool res = true;
        for (int i = 0; i < M; i++) {
            if (lhs[i] != rhs[i]) {
                res = false;
                break;
            }
        }
        return res;
    }

    Mat<T, M, N> &operator+=(const Mat<T, M, N> &rhs) {
        for (int i = 0; i < M; i++) {
            this->data_[i] += rhs.data_[i];
        }
        return *this;
    }

    Mat<T, M, N> &operator-=(const Mat<T, M, N> &rhs) {
        for (int i = 0; i < M; i++) {
            this->data_[i] -= rhs.data_[i];
        }
        return *this;
    }

    Mat<T, M, N> &operator*=(const Mat<T, M, N> &rhs) {
        (*this) = (*this) * rhs;
        return *this;
    }

    Mat<T, M, N> &operator/=(const Mat<T, M, N> &rhs) {
        for (int i = 0; i < M; i++) {
            this->data_[i] /= rhs.data_[i];
        }
        return *this;
    }

    Mat<T, M, N> &operator*=(T rhs) {
        for (int i = 0; i < M; i++) {
            this->data_[i] *= rhs;
        }
        return *this;
    }

    Mat<T, M, N> &operator/=(T rhs) {
        for (int i = 0; i < M; i++) {
            this->data_[i] /= rhs;
        }
        return *this;
    }

    friend Mat<T, M, N> operator-(const Mat<T, M, N> &v) {
        Mat<T, M, N> res = { Uninitialize };
        for (int i = 0; i < M; i++) {
            res.data_[i] = -v.data_[i];
        }
        return res;
    }

    friend Mat<T, M, N> operator+(const Mat<T, M, N> &lhs, const Mat<T, M, N> &rhs) {
        Mat<T, M, N> res = { Uninitialize };
        for (int i = 0; i < M; i++) {
            res.data_[i] = lhs.data_[i] + rhs.data_[i];
        }
        return res;
    }

    friend Mat<T, M, N> operator-(const Mat<T, M, N> &lhs, const Mat<T, M, N> &rhs) {
        Mat<T, M, N> res = { Uninitialize };
        for (int i = 0; i < M; i++) {
            res.data_[i] = lhs.data_[i] - rhs.data_[i];
        }
        return res;
    }

    friend Mat<T, M, N> operator/(const Mat<T, M, N> &lhs, const Mat<T, M, N> &rhs) {
        Mat<T, M, N> res = { Uninitialize };
        for (int i = 0; i < M; i++) {
            res.data_[i] = lhs.data_[i] / rhs.data_[i];
        }
        return res;
    }

    friend Mat<T, M, N> operator*(T lhs, const Mat<T, M, N> &rhs) {
        Mat<T, M, N> res = { Uninitialize };
        for (int i = 0; i < M; i++) {
            res.data_[i] = lhs * rhs.data_[i];
        }
        return res;
    }

    friend Mat<T, M, N> operator/(T lhs, const Mat<T, M, N> &rhs) {
        Mat<T, M, N> res = { Uninitialize };
        for (int i = 0; i < M; i++) {
            res.data_[i] = lhs / rhs.data_[i];
        }
        return res;
    }

    friend Mat<T, M, N> operator*(const Mat<T, M, N> &lhs, const T &rhs) {
        Mat<T, M, N> res = { Uninitialize };
        for (int i = 0; i < M; i++) {
            res.data_[i] = lhs.data_[i] * rhs;
        }
        return res;
    }

    friend Mat<T, M, N> operator/(const Mat<T, M, N> &lhs, const T &rhs) {
        Mat<T, M, N> res = { Uninitialize };
        for (int i = 0; i < M; i++) {
            res.data_[i] = lhs.data_[i] / rhs;
        }
        return res;
    }
};

using Mat2f = Mat<float, 2, 2>;
using Mat2d = Mat<double, 2, 2>;
using Mat3f = Mat<float, 3, 3>;
using Mat3d = Mat<double, 3, 3>;
using Mat4f = Mat<float, 4, 4>;
using Mat4d = Mat<double, 4, 4>;
using Mat4x3f = Mat<float, 4, 3>;
using Mat4x3d = Mat<double, 4, 3>;
using Mat3x4f = Mat<float, 3, 4>;
using Mat3x4d = Mat<double, 3, 4>;

template <typename T, int M, int N>
Vec<T, M> operator*(const Vec<T, M> &lhs, const Mat<T, M, N> &rhs) {
    auto res = Vec<T, M>{ Uninitialize };
    for (int m = 0; m < M; m++) {
        res[m] = Dot(lhs, rhs[m]);
    }
    return res;
}

template <typename T, int M, int N>
Vec<T, M> operator*(const Mat<T, M, N> &lhs, const Vec<T, M> &rhs) {
    auto res = Vec<T, M>{ Uninitialize };
    for (int n = 0; n < N; n++) {
        T sum = (T)0;
        for (int m = 0; m < M; m++) {
            sum += lhs[m][n] * rhs[m];
        }
        res[n] = sum;
    }
    return res;
}

template <typename T, int M, int N, int P>
Mat<T, M, P> operator*(const Mat<T, M, N> &lhs, const Mat<T, N, P> &rhs) {
    Mat<T, M, P> res = { Uninitialize };
    for (int m = 0; m < M; m++) {
        for (int p = 0; p < P; p++) {
            T sum = (T)0;
            for (int n = 0; n < N; n++) {
                sum += rhs[m][n] * lhs[n][p];
            }
            res[m][p] = sum;
        }
    }
    return res;
}

template <typename T, int M, int N>
Mat<T, N, M> Transpose(const Mat<T, M, N> &mat) {
    Mat<T, N, M> res = { Uninitialize };
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            res[n][m] = mat[m][n];
        }
    }
    return res;
}

template <typename T, int N>
T Det(const Mat<T, N, N> &mat);

template <typename T, int N>
T Minor(const Mat<T, N, N> &mat, int row, int col) {
    Mat<T, N - 1, N - 1> res = { Uninitialize };
    int dst_row, dst_col;
    dst_row = 0;
    for (int src_row = 0; src_row < N; src_row++) {
        if (src_row == row) continue;
        dst_col = 0;
        for (int src_col = 0; src_col < N; src_col++) {
            if (src_col == col) continue;
            res[dst_row][dst_col] = mat[src_row][src_col];
            dst_col++;
        }
        dst_row++;
    }
    return Det(res);
}

template <typename T, int N>
T Det(const Mat<T, N, N> &mat) {
    T sum = (T)0;
    for (unsigned n = 0; n < N; n++) {
        T minor = Minor(mat, n, 0);
        T cofactor = (n & 1u) ? -minor : minor;
        sum += mat[n][0] * cofactor;
    }
    return sum;
}

template <typename T>
T Det(const Mat<T, 1, 1> &mat) {
    return mat[0][0];
}

template <typename T>
T Det(const Mat<T, 2, 2> &mat) {
    return mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];
}

template <typename T>
T Det(const Mat<T, 3, 3> &mat) {
    return mat[0][0] * (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) -
           mat[1][0] * (mat[0][1] * mat[2][2] - mat[2][1] * mat[0][2]) +
           mat[2][0] * (mat[0][1] * mat[1][2] - mat[1][1] * mat[0][2]);
}

template <typename T>
T Det(const Mat<T, 4, 4> &mat) {
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

    return mat[0][0] * minor0 - mat[1][0] * minor1 + mat[2][0] * minor2 - mat[3][0] * minor3;
}

template <typename T, int N>
Mat<T, N, N> Adj(const Mat<T, N, N> &mat) {
    Mat<T, N, N> res = { Uninitialize };
    for (unsigned row = 0; row < N; row++) {
        for (unsigned col = 0; col < N; col++) {
            T minor = Minor(mat, row, col);
            T cofactor = ((row + col) & 1u) ? -minor : minor;
            res[col][row] = cofactor;
        }
    }
    return res;
}

template <typename T, int N>
Mat<T, N, N> Inverse(const Mat<T, N, N> &mat) {
    T det = Det(mat);
    return (T(1) / det) * Adj(mat);
}

template <typename T>
Mat<T, 2, 2> Inverse(const Mat<T, 2, 2> &mat) {
    T det = Det(mat);
    T inv_det = T(1) / det;
    Mat<T, 2, 2> res = { Uninitialize };
    res[0][0] = inv_det * mat[1][1];
    res[0][1] = inv_det * -mat[0][1];
    res[1][0] = inv_det * -mat[1][0];
    res[1][1] = inv_det * mat[0][0];
    return res;
}

template <typename T>
Mat<T, 3, 3> Inverse(const Mat<T, 3, 3> &mat) {
    T minor0 = mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2];
    T minor1 = mat[0][1] * mat[2][2] - mat[2][1] * mat[0][2];
    T minor2 = mat[0][1] * mat[1][2] - mat[1][1] * mat[0][2];

    T det = mat[0][0] * minor0 - mat[1][0] * minor1 + mat[2][0] * minor2;
    T inv_det = T(1) / det;

    Mat<T, 3, 3> res = { Uninitialize };
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

template <typename T>
Mat<T, 4, 4> Inverse(const Mat<T, 4, 4> &mat) {
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
    T det = mat[0][0] * minor0 - mat[1][0] * minor1 + mat[2][0] * minor2 - mat[3][0] * minor3;
    T inv_det = T(1) / det;

    Mat<T, 4, 4> res = { Uninitialize };
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

template <typename T>
Mat<T, 4, 4> Translate(const Mat<T, 4, 4> &m, const Vec<T, 3> &v) {
    Mat<T, 4, 4> res = m;
    res[3] += m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
    return res;
}

template <typename T>
Mat<T, 4, 4> Rotate(const Mat<T, 4, 4> &m, T angle_rad, const Vec<T, 3> &_axis) {
    const T a = angle_rad;
    const T c = std::cos(a);
    const T s = std::sin(a);

    Vec<T, 3> axis = Normalize(_axis);
    Vec<T, 3> temp = (T(1) - c) * axis;

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

template <typename T>
Mat<T, 4, 4> Scale(const Mat<T, 4, 4> &m, const Vec<T, 3> &v) {
    Mat<T, 4, 4> res(Uninitialize);
    res[0] = m[0] * v[0];
    res[1] = m[1] * v[1];
    res[2] = m[2] * v[2];
    res[3] = m[3];
    return res;
}

template <typename T, int M, int N>
const T *ValuePtr(const Mat<T, M, N> &v) {
    return &v[0][0];
}

template <typename T, int M, int N>
const T *ValuePtr(const Mat<T, M, N> *v) {
    return &(*v)[0][0];
}

/*template <typename T>
Mat<T, 3, 3> MakeMat3(T v) {
    return Mat<T, 3, 3>{ Vec<T, 3>{ v, T(0), T(0) },
                         Vec<T, 3>{ T(0), v, T(0) },
                         Vec<T, 3>{ T(0), T(0), v } };
}

template <typename T>
Mat<T, 4, 4> MakeMat4(T v) {
    return Mat<T, 4, 4>{ Vec<T, 4>{ v,    T(0), T(0), T(0) },
                         Vec<T, 4>{ T(0), v,    T(0), T(0) },
                         Vec<T, 4>{ T(0), T(0), v,    T(0) },
                         Vec<T, 4>{ T(0), T(0), T(0), v    } };
}*/

template <typename T>
void LookAt(Mat<T, 4, 4> &m, const Vec<T, 3> &src, const Vec<T, 3> &trg, const Vec<T, 3> &up) {
    Vec<T, 3> f = Normalize(trg - src);
    Vec<T, 3> s = Normalize(Cross(f, up));
    Vec<T, 3> u = Cross(s, f);

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
void PerspectiveProjection(Mat<T, 4, 4> &m, T fov, T aspect, T znear, T zfar) {
    T xymax = znear * std::tan(fov * Pi<T>() / T(360));
    T ymin = -xymax;
    T xmin = -xymax;

    T width = xymax - xmin;
    T height = xymax - ymin;

    T depth = zfar - znear;
    T q = -(zfar + znear) / depth;
    T qn = -2 * (zfar * znear) / depth;

    T w = 2 * znear / width;
    w = w / aspect;
    T h = 2 * znear / height;

    m[0][0] = w;
    m[0][1] = m[0][2] = m[0][3] = T(0);

    m[1][1] = h;
    m[1][0] = m[1][2] = m[1][3] = T(0);

    m[2][0] = m[2][1] = T(0);
    m[2][2] = q;
    m[2][3] = T(-1);

    m[3][0] = m[3][1] = T(0);
    m[3][2] = qn;
    m[3][3] = T(0);
}

template <typename T>
void OrthographicProjection(Mat<T, 4, 4> &m, T left, T right, T bottom, T top, T nnear, T ffar) {
    /*T r_width = T(1) / (right - left);
    T r_height = T(1) / (top - bottom);
    T r_depth = T(1) / (nnear - ffar);
    T x = T(2) * (nnear * r_width);
    T y = T(2) * (nnear * r_height);
    T A = ((right + left) * r_width);
    T B = (top + bottom) * r_height;
    T C = (ffar + nnear) * r_depth;
    T D = T(2) * (ffar * nnear * r_depth);

    m = Mat<T, 4, 4>{ T(0) };

    m[0][0] = x;
    m[1][1] = y;
    m[2][0] = A;
    m[2][1] = B;
    m[2][2] = C;
    m[3][2] = D;
    m[2][3] = -T(1);*/

    T r_width = T(1) / (right - left);
    T r_height = T(1) / (top - bottom);
    T r_depth = T(1) / (ffar - nnear);

    m = Mat<T, 4, 4> { T(0) };

    m[0][0] = T(2) * r_width;
    m[1][1] = T(2) * r_height;
    m[2][2] = -T(2) * r_depth;
    m[3][0] = -(right + left) * r_width;
    m[3][1] = -(top + bottom) * r_height;
    m[3][2] = -(ffar + nnear) * r_depth;
    m[3][3] = T(1);
}
}