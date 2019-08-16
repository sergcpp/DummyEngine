#include "test_common.h"

#include "../MMat.h"

void test_mat() {
    using namespace Ren;

    {
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
}