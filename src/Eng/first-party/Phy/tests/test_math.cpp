#include "test_common.h"

#include "../Mat.h"
#include "../Quat.h"

void test_math() {
    using namespace Phy;

    printf("Test math               | ");

    { // vector usage (1)
        auto v1 = Vec2f{1.0f, 2.0f}, v2 = Vec2f{3.0f, 4.0f};

        require(v1[0] == 1.0f);
        require(v1[1] == 2.0f);
        require(v2[0] == 3.0f);
        require(v2[1] == 4.0f);

        auto v3 = Vec2f{0.0f};

        require(v3[0] == 0.0f);
        require(v3[1] == 0.0f);

        require(v1 != v3);

        v3[0] = 1.0f;
        v3[1] = 2.0f;

        require(v1 == v3);

        v3 = v1 + v2;

        require(v3[0] == Approx(4.0f));
        require(v3[1] == Approx(6.0f));

        v3 = v1 - v2;

        require(v3[0] == Approx(-2.0f));
        require(v3[1] == Approx(-2.0f));

        v3 = v1 * v2;

        require(v3[0] == Approx(3.0f));
        require(v3[1] == Approx(8.0f));

        v3 = v1 / v2;

        require(v3[0] == Approx(1.0f / 3.0f));
        require(v3[1] == Approx(2.0f / 4.0f));

        v3 = 2.0f * v1;

        require(v3[0] == Approx(2.0f));
        require(v3[1] == Approx(4.0f));

        v3 = v1 / 2.0f;

        require(v3[0] == Approx(0.5f));
        require(v3[1] == Approx(1.0f));
    }

    { // vector usage (2)
        auto v1 = Vec3f{1.0f, 2.0f, 3.0f}, v2 = Vec3f{3.0f, 4.0f, 5.0f};

        require(v1[0] == 1.0f);
        require(v1[1] == 2.0f);
        require(v1[2] == 3.0f);
        require(v2[0] == 3.0f);
        require(v2[1] == 4.0f);
        require(v2[2] == 5.0f);

        auto v3 = Vec3f{0.0f};

        require(v3[0] == 0.0f);
        require(v3[1] == 0.0f);
        require(v3[2] == 0.0f);

        require(v1 != v3);

        v3[0] = 1.0f;
        v3[1] = 2.0f;
        v3[2] = 3.0f;

        require(v1 == v3);

        v3 = v1 + v2;

        require(v3[0] == Approx(4.0f));
        require(v3[1] == Approx(6.0f));
        require(v3[2] == Approx(8.0f));

        v3 = v1 - v2;

        require(v3[0] == Approx(-2.0f));
        require(v3[1] == Approx(-2.0f));
        require(v3[2] == Approx(-2.0f));

        v3 = v1 * v2;

        require(v3[0] == Approx(3.0f));
        require(v3[1] == Approx(8.0f));
        require(v3[2] == Approx(15.0f));

        v3 = v1 / v2;

        require(v3[0] == Approx(1.0f / 3.0f));
        require(v3[1] == Approx(2.0f / 4.0f));
        require(v3[2] == Approx(3.0f / 5.0f));

        v3 = 2.0f * v1;

        require(v3[0] == Approx(2.0f));
        require(v3[1] == Approx(4.0f));
        require(v3[2] == Approx(6.0f));

        v3 = v1 / 2.0f;

        require(v3[0] == Approx(0.5f));
        require(v3[1] == Approx(1.0f));
        require(v3[2] == Approx(1.5f));
    }

    { // vector usage (3)
        auto v1 = Vec4f{1.0f, 2.0f, 3.0f, 4.0f}, v2 = Vec4f{3.0f, 4.0f, 5.0f, 6.0f};

        require(v1[0] == 1.0f);
        require(v1[1] == 2.0f);
        require(v1[2] == 3.0f);
        require(v1[3] == 4.0f);
        require(v2[0] == 3.0f);
        require(v2[1] == 4.0f);
        require(v2[2] == 5.0f);
        require(v2[3] == 6.0f);

        auto v3 = Vec4f{0.0f};

        require(v3[0] == 0.0f);
        require(v3[1] == 0.0f);
        require(v3[2] == 0.0f);
        require(v3[3] == 0.0f);

        require(v1 != v3);

        v3[0] = 1.0f;
        v3[1] = 2.0f;
        v3[2] = 3.0f;
        v3[3] = 4.0f;

        require(v1 == v3);

        v3 = v1 + v2;

        require(v3[0] == Approx(4.0f));
        require(v3[1] == Approx(6.0f));
        require(v3[2] == Approx(8.0f));
        require(v3[3] == Approx(10.0f));

        v3 = v1 - v2;

        require(v3[0] == Approx(-2.0f));
        require(v3[1] == Approx(-2.0f));
        require(v3[2] == Approx(-2.0f));
        require(v3[3] == Approx(-2.0f));

        v3 = v1 * v2;

        require(v3[0] == Approx(3.0f));
        require(v3[1] == Approx(8.0f));
        require(v3[2] == Approx(15.0f));
        require(v3[3] == Approx(24.0f));

        v3 = v1 / v2;

        require(v3[0] == Approx(1.0f / 3.0f));
        require(v3[1] == Approx(2.0f / 4.0f));
        require(v3[2] == Approx(3.0f / 5.0f));
        require(v3[3] == Approx(4.0f / 6.0f));

        v3 = 2.0f * v1;

        require(v3[0] == Approx(2.0f));
        require(v3[1] == Approx(4.0f));
        require(v3[2] == Approx(6.0f));
        require(v3[3] == Approx(8.0f));

        v3 = v1 / 2.0f;

        require(v3[0] == Approx(0.5f));
        require(v3[1] == Approx(1.0f));
        require(v3[2] == Approx(1.5f));
        require(v3[3] == Approx(2.0f));
    }

    { // matrix usage

        auto v1 = Mat2f{Vec2f{1.0f, 2.0f}, //
                        Vec2f{3.0f, 4.0f}},
             v2 = Mat2f{Vec2f{3.0f, 4.0f}, //
                        Vec2f{5.0f, 6.0f}};

        require(v1[0][0] == 1.0f);
        require(v1[0][1] == 2.0f);
        require(v1[1][0] == 3.0f);
        require(v1[1][1] == 4.0f);

        require(v2[0][0] == 3.0f);
        require(v2[0][1] == 4.0f);
        require(v2[1][0] == 5.0f);
        require(v2[1][1] == 6.0f);

        auto v3 = Mat2f{Vec2f{0.0f}, Vec2f{0.0f}};

        require(v3[0][0] == 0.0f);
        require(v3[0][1] == 0.0f);
        require(v3[1][0] == 0.0f);
        require(v3[1][1] == 0.0f);

        require(v1 != v3);

        v3[0][0] = 1.0f;
        v3[0][1] = 2.0f;
        v3[1][0] = 3.0f;
        v3[1][1] = 4.0f;

        require(v1 == v3);

        v3 = v1 + v2;

        require(v3[0][0] == Approx(4.0f));
        require(v3[0][1] == Approx(6.0f));
        require(v3[1][0] == Approx(8.0f));
        require(v3[1][1] == Approx(10.0f));

        v3 = v1 - v2;

        require(v3[0][0] == Approx(-2.0f));
        require(v3[0][1] == Approx(-2.0f));
        require(v3[1][0] == Approx(-2.0f));
        require(v3[1][1] == Approx(-2.0f));

        v3 = v1 * v2;

        require(v3[0][0] == Approx(15.0f));
        require(v3[0][1] == Approx(22.0f));
        require(v3[1][0] == Approx(23.0f));
        require(v3[1][1] == Approx(34.0f));
    }

    { // vec dot product
        auto v1 = Vec3f{1.0f, 2.0f, 3.0f};
        auto v2 = Vec3f{4.0f, 5.0f, 6.0f};

        require(Dot(v1, v2) == Approx(32.0f));
        require(Dot(v1, Vec3f{0.0f}) == Approx(0.0f));
    }

    { // vec cross product
        auto v1 = Vec3f{1.0f, 2.0f, 3.0f};
        auto v2 = Vec3f{4.0f, 5.0f, 6.0f};

        auto c = Cross(v1, v2);
        require(c[0] == Approx(-3.0f));
        require(c[1] == Approx(6.0f));
        require(c[2] == Approx(-3.0f));

        require(std::abs(Dot(c, v1)) < 0.001f);
        require(std::abs(Dot(c, v2)) < 0.001f);

        // cross of parallel vectors is zero
        auto z = Cross(v1, v1);
        require(z[0] == Approx(0.0f));
        require(z[1] == Approx(0.0f));
        require(z[2] == Approx(0.0f));
    }

    { // vec length and normalize
        auto v = Vec3f{3.0f, 4.0f, 0.0f};
        require(Length(v) == Approx(5.0f));
        require(Distance(v, Vec3f{0.0f}) == Approx(5.0f));

        auto n = Normalize(v);
        require(n[0] == Approx(0.6f));
        require(n[1] == Approx(0.8f));
        require(n[2] == Approx(0.0f));
        require(Length(n) == Approx(1.0f));
    }

    { // vec Mix, Clamp, Saturate, Step
        require(Mix(0.0f, 10.0f, 0.0f) == Approx(0.0f));
        require(Mix(0.0f, 10.0f, 1.0f) == Approx(10.0f));
        require(Mix(0.0f, 10.0f, 0.3f) == Approx(3.0f));

        auto vm = Mix(Vec3f{0.0f}, Vec3f{4.0f}, 0.5f);
        require(vm[0] == Approx(2.0f));
        require(vm[1] == Approx(2.0f));
        require(vm[2] == Approx(2.0f));

        require(Clamp(1.5f, 0.0f, 1.0f) == Approx(1.0f));
        require(Clamp(-0.5f, 0.0f, 1.0f) == Approx(0.0f));
        require(Clamp(0.5f, 0.0f, 1.0f) == Approx(0.5f));

        require(Saturate(1.5f) == Approx(1.0f));
        require(Saturate(-0.5f) == Approx(0.0f));
        require(Saturate(0.7f) == Approx(0.7f));

        auto s = Step(Vec3f{0.5f, 0.5f, 0.5f}, Vec3f{0.3f, 0.5f, 0.7f});
        require(s[0] == Approx(0.0f));
        require(s[1] == Approx(1.0f));
        require(s[2] == Approx(1.0f));
    }

    { // vec Min, Max
        auto a = Vec3f{1.0f, 4.0f, 2.0f};
        auto b = Vec3f{3.0f, 2.0f, 5.0f};

        auto mn = Min(a, b);
        require(mn[0] == Approx(1.0f));
        require(mn[1] == Approx(2.0f));
        require(mn[2] == Approx(2.0f));

        auto mx = Max(a, b);
        require(mx[0] == Approx(3.0f));
        require(mx[1] == Approx(4.0f));
        require(mx[2] == Approx(5.0f));

        auto mn_s = Min(a, 2.0f);
        require(mn_s[0] == Approx(1.0f));
        require(mn_s[1] == Approx(2.0f));
        require(mn_s[2] == Approx(2.0f));
    }

    { // AbsFloor regression (was reading uninitialized ret[i] instead of v[i])
        auto r = AbsFloor(Vec4f{1.7f, -1.7f, 2.9f, -2.9f});
        require(r[0] == Approx(1.0f));
        require(r[1] == Approx(-1.0f));
        require(r[2] == Approx(2.0f));
        require(r[3] == Approx(-2.0f));
    }

    { // mat3 identity constructor and inverse
        auto identity = Mat3f{};
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                require(identity[i][j] == Approx(i == j ? 1.0f : 0.0f));
            }
        }

        auto m = Mat3f{Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{0.0f, 1.0f, 4.0f}, Vec3f{5.0f, 6.0f, 0.0f}};
        auto prod3 = m * Inverse(m);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                require(prod3[i][j] == Approx(i == j ? 1.0f : 0.0f));
            }
        }
    }

    { // mat4 identity, Translate + Inverse
        auto identity = Mat4f{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                require(identity[i][j] == Approx(i == j ? 1.0f : 0.0f));
            }
        }

        auto m = Translate(Mat4f{}, Vec3f{3.0f, -1.0f, 2.0f});
        auto prod4 = m * Inverse(m);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                require(prod4[i][j] == Approx(i == j ? 1.0f : 0.0f));
            }
        }
    }

    { // mat transpose
        auto a = Mat3f{Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{4.0f, 5.0f, 6.0f}, Vec3f{7.0f, 8.0f, 9.0f}};
        auto at = Transpose(a);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                require(at[i][j] == Approx(a[j][i]));
            }
        }
        require(Transpose(at) == a);
    }

    { // non-square matrix multiply regression (index swap fix)
        // Mat3x4f (3 rows, 4 cols) * Mat4x3f (4 rows, 3 cols) = Mat3f (3x3)
        auto a = Mat3x4f{Vec4f{1.0f, 0.0f, 0.0f, 0.0f},
                         Vec4f{0.0f, 1.0f, 0.0f, 0.0f},
                         Vec4f{0.0f, 0.0f, 1.0f, 0.0f}};
        auto b = Mat4x3f{Vec3f{2.0f, 0.0f, 0.0f},
                         Vec3f{0.0f, 3.0f, 0.0f},
                         Vec3f{0.0f, 0.0f, 4.0f},
                         Vec3f{5.0f, 6.0f, 7.0f}};
        auto r = a * b;
        require(r[0][0] == Approx(2.0f)); require(r[0][1] == Approx(0.0f)); require(r[0][2] == Approx(0.0f));
        require(r[1][0] == Approx(0.0f)); require(r[1][1] == Approx(3.0f)); require(r[1][2] == Approx(0.0f));
        require(r[2][0] == Approx(0.0f)); require(r[2][1] == Approx(0.0f)); require(r[2][2] == Approx(4.0f));
    }

    { // mat * vec with Translate and Scale
        auto t = Translate(Mat4f{}, Vec3f{1.0f, 2.0f, 3.0f});
        auto vt = t * Vec4f{5.0f, 6.0f, 7.0f, 1.0f};
        require(vt[0] == Approx(6.0f));
        require(vt[1] == Approx(8.0f));
        require(vt[2] == Approx(10.0f));
        require(vt[3] == Approx(1.0f));

        auto s = Scale(Mat4f{}, Vec3f{2.0f, 3.0f, 4.0f});
        auto vs = s * Vec4f{5.0f, 6.0f, 7.0f, 1.0f};
        require(vs[0] == Approx(10.0f));
        require(vs[1] == Approx(18.0f));
        require(vs[2] == Approx(28.0f));
        require(vs[3] == Approx(1.0f));
    }

    { // quat default constructor is identity
        auto q = Quatf{};
        require(q.x == 0.0f);
        require(q.y == 0.0f);
        require(q.z == 0.0f);
        require(q.w == 1.0f);

        auto m = ToMat3(q);
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                require(m[i][j] == Approx(i == j ? 1.0f : 0.0f));
            }
        }
    }

    { // quat axis-angle construction and ToMat3 rotation
        // 90 degrees around Z: rotating X-axis should give Y-axis
        auto q = Quatf{Vec3f{0.0f, 0.0f, 1.0f}, Pi<float>() / 2.0f};
        const float half_sqrt2 = std::sqrt(2.0f) / 2.0f;
        require(q.x == Approx(0.0f));
        require(q.y == Approx(0.0f));
        require(q.z == Approx(half_sqrt2));
        require(q.w == Approx(half_sqrt2));

        auto rotated = ToMat3(q) * Vec3f{1.0f, 0.0f, 0.0f};
        require(rotated[0] == Approx(0.0f));
        require(rotated[1] == Approx(1.0f));
        require(rotated[2] == Approx(0.0f));
    }

    { // quat Hamilton product non-commutativity
        auto qx = Quatf{Vec3f{1.0f, 0.0f, 0.0f}, Pi<float>() / 2.0f};
        auto qy = Quatf{Vec3f{0.0f, 1.0f, 0.0f}, Pi<float>() / 2.0f};

        auto qxy = qx * qy;
        auto qyx = qy * qx;

        require(qxy.z == Approx(0.5f));
        require(qyx.z == Approx(-0.5f));
    }

    { // quat Normalize and Inverse
        auto q = Normalize(Quatf{0.5f, 0.5f, 0.5f, 0.5f});
        require(Dot(q, q) == Approx(1.0f));

        auto q_unit = Quatf{Vec3f{1.0f, 0.0f, 0.0f}, Pi<float>() / 3.0f};
        auto prod = q_unit * Inverse(q_unit);
        require(prod.x == Approx(0.0f));
        require(prod.y == Approx(0.0f));
        require(prod.z == Approx(0.0f));
        require(prod.w == Approx(1.0f));
    }

    { // quat Slerp
        auto q0 = Quatf{};
        auto q1 = Quatf{Vec3f{0.0f, 0.0f, 1.0f}, Pi<float>() / 2.0f};

        auto r0 = Slerp(q0, q1, 0.0f);
        require(r0.x == Approx(0.0f));
        require(r0.y == Approx(0.0f));
        require(r0.z == Approx(0.0f));
        require(r0.w == Approx(1.0f));

        auto r1 = Slerp(q0, q1, 1.0f);
        require(r1.x == Approx(0.0f));
        require(r1.y == Approx(0.0f));
        require(r1.z == Approx(q1.z));
        require(r1.w == Approx(q1.w));

        // t=0.5 should give 45 degrees around Z
        auto r_half = Slerp(q0, q1, 0.5f);
        require(r_half.x == Approx(0.0f));
        require(r_half.y == Approx(0.0f));
        require(r_half.z == Approx(std::sin(Pi<float>() / 8.0f)));
        require(r_half.w == Approx(std::cos(Pi<float>() / 8.0f)));
    }

    { // Yaw regression (was calling undefined clamp(), now uses Clamp())
        // 90 degrees around Y axis: Yaw should return pi/2
        auto q = Quatf{Vec3f{0.0f, 1.0f, 0.0f}, Pi<float>() / 2.0f};
        require(Yaw(q) == Approx(Pi<float>() / 2.0f));

        // zero rotation: all Euler angles should be zero
        auto angles = EulerAngles(Quatf{});
        require(angles[0] == Approx(0.0f));
        require(angles[1] == Approx(0.0f));
        require(angles[2] == Approx(0.0f));
    }

    printf("OK\n");
}
