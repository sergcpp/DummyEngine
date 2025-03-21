#include "test_common.h"

#include "../MMat.h"

void test_math() {
    using namespace Ren;

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
        auto v1 = Mat2f{Vec2f{1.0f, 2.0f}, Vec2f{3.0f, 4.0f}}, v2 = Mat2f{Vec2f{3.0f, 4.0f}, Vec2f{5.0f, 6.0f}};

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

    printf("OK\n");
}