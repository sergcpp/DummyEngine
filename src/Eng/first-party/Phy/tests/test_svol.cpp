#include "test_common.h"

#include "../Utils.h"

void test_svol() {
    using namespace Phy;

    printf("Test svol               | ");

    static const Vec3 org_pts[] = {Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)};

    { // origin is outside
        Vec3 pts[4];
        for (int i = 0; i < 4; i++) {
            pts[i] = org_pts[i] + Vec3(1, 1, 1);
        }

        const Vec4 lambdas = SignedVolume3D(pts[0], pts[1], pts[2], pts[3]);
        auto v = Vec3{0};
        for (int i = 0; i < 4; i++) {
            v += pts[i] * lambdas[i];
        }

        require(lambdas[0] == Approx(1.0));
        require(lambdas[1] == Approx(0.0));
        require(lambdas[2] == Approx(0.0));
        require(lambdas[3] == Approx(0.0));

        require(v[0] == Approx(1.0));
        require(v[1] == Approx(1.0));
        require(v[2] == Approx(1.0));
    }

    { // origin is in center
        Vec3 pts[4];
        for (int i = 0; i < 4; i++) {
            pts[i] = org_pts[i] + Vec3(-1) * real(0.25);
        }

        const Vec4 lambdas = SignedVolume3D(pts[0], pts[1], pts[2], pts[3]);
        auto v = Vec3{0};
        for (int i = 0; i < 4; i++) {
            v += pts[i] * lambdas[i];
        }

        require(lambdas[0] == Approx(0.25));
        require(lambdas[1] == Approx(0.25));
        require(lambdas[2] == Approx(0.25));
        require(lambdas[3] == Approx(0.25));

        require(v[0] == Approx(0.0));
        require(v[1] == Approx(0.0));
        require(v[2] == Approx(0.0));
    }

    { // origin is on the surface
        Vec3 pts[4];
        for (int i = 0; i < 4; i++) {
            pts[i] = org_pts[i] + Vec3(-1);
        }

        const Vec4 lambdas = SignedVolume3D(pts[0], pts[1], pts[2], pts[3]);
        auto v = Vec3{0};
        for (int i = 0; i < 4; i++) {
            v += pts[i] * lambdas[i];
        }

        require(lambdas[0] == Approx(0.0));
        require(lambdas[1] == Approx(0.333));
        require(lambdas[2] == Approx(0.333));
        require(lambdas[3] == Approx(0.333));

        require(v[0] == Approx(-0.667));
        require(v[1] == Approx(-0.667));
        require(v[2] == Approx(-0.667));
    }

    { // origin is on the edge
        Vec3 pts[4];
        for (int i = 0; i < 4; i++) {
            pts[i] = org_pts[i] + Vec3(1, 1, real(-0.5));
        }

        const Vec4 lambdas = SignedVolume3D(pts[0], pts[1], pts[2], pts[3]);
        auto v = Vec3{0};
        for (int i = 0; i < 4; i++) {
            v += pts[i] * lambdas[i];
        }

        require(lambdas[0] == Approx(0.5));
        require(lambdas[1] == Approx(0.0));
        require(lambdas[2] == Approx(0.0));
        require(lambdas[3] == Approx(0.5));

        require(v[0] == Approx(1.0));
        require(v[1] == Approx(1.0));
        require(v[2] == Approx(0.0));
    }

    { // origin is at (0, 0, 0)
        static const Vec3 pts[] = {Vec3(real(+51.1996613), real(+26.1989613), real(1.91339576)),
                                   Vec3(real(-51.0567360), real(-26.0565681), real(-0.436143428)),
                                   Vec3(real(+50.8978920), real(-24.1035538), real(-1.04042661)),
                                   Vec3(real(-49.1021080), real(+25.8964462), real(-1.04042661))};

        const Vec4 lambdas = SignedVolume3D(pts[0], pts[1], pts[2], pts[3]);
        auto v = Vec3{0};
        for (int i = 0; i < 4; i++) {
            v += pts[i] * lambdas[i];
        }

        require(lambdas[0] == Approx(0.290));
        require(lambdas[1] == Approx(0.302));
        require(lambdas[2] == Approx(0.206));
        require(lambdas[3] == Approx(0.202));

        require(v[0] == Approx(0.0));
        require(v[1] == Approx(0.0));
        require(v[2] == Approx(0.0));
    }

    printf("OK\n");
}