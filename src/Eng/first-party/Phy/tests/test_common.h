#pragma once

#include <cmath>
#include <cstdlib>

#include <string>

static void handle_assert(bool passed, const char* assert, const char* file, long line) {
    if (!passed) {
        printf("Assertion failed %s in %s at line %d\n", assert, file, int(line));
        exit(-1);
    }
}

#define require(x) handle_assert(x, #x , __FILE__, __LINE__ )

#define require_throws(expr) {          \
            bool _ = false;             \
            try {                       \
                expr;                   \
            } catch (...) {             \
                _ = true;               \
            }                           \
            require(_);                 \
        }

#define require_nothrow(expr) {         \
            bool _ = false;             \
            try {                       \
                expr;                   \
            } catch (...) {             \
                _ = true;               \
            }                           \
            require(!_);                \
        }

class Approx {
public:
    explicit Approx(double val) : val(val), eps(0.001) {
        require(eps > 0);
    }

    const Approx &epsilon(double _eps) {
        eps = _eps;
        return *this;
    }

    double val, eps;
};

inline bool operator==(double val, const Approx &app) {
    return std::abs(val - app.val) < app.eps;
}

inline bool operator==(float val, const Approx &app) {
    return std::abs(val - app.val) < app.eps;
}
