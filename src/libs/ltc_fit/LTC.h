#ifndef _LTC_
#define _LTC_

#include "MMat.h"

#include <iostream>
using namespace std;

struct LTC {

    // lobe magnitude
    float magnitude;

    // Average Schlick Fresnel term
    float fresnel;

    // parametric representation
    float m11, m22, m13;
    Vec3f X, Y, Z;

    // matrix representation
    Mat3f M;
    Mat3f invM;
    float detM;

    LTC() {
        magnitude = 1;
        fresnel = 1;
        m11 = 1;
        m22 = 1;
        m13 = 0;
        X = Vec3f(1, 0, 0);
        Y = Vec3f(0, 1, 0);
        Z = Vec3f(0, 0, 1);
        update();
    }

    void update() // compute matrix from parameters
    {
        M = Mat3f(X, Y, Z) * Mat3f(Vec3f{m11, 0, 0}, Vec3f{0, m22, 0}, Vec3f{m13, 0, 1});
        invM = Inverse(M);
        detM = abs(Det(M));
    }

    float eval(const Vec3f &L) const {
        Vec3f Loriginal = Normalize(invM * L);
        Vec3f L_ = M * Loriginal;

        float l = Length(L_);
        float Jacobian = detM / (l * l * l);

        float D = 1.0f / 3.14159f * std::max<float>(0.0f, Loriginal[2]);

        float res = magnitude * D / Jacobian;
        return res;
    }

    Vec3f sample(const float U1, const float U2) const {
        const float theta = acosf(sqrtf(U1));
        const float phi = 2.0f * 3.14159f * U2;
        const Vec3f L = Normalize(M * Vec3f(sinf(theta) * cosf(phi), sinf(theta) * sinf(phi), cosf(theta)));
        return L;
    }
};

#endif
