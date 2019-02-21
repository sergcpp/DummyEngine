#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#undef NDEBUG
#include <cassert>
#include <cmath>

#define assert_false(expr) assert(!expr)

#define assert_throws(expr) {           \
            bool _ = false;             \
            try {                       \
                expr;                   \
            } catch (...) {             \
                _ = true;               \
            }                           \
            assert(_);                  \
        }

#define assert_nothrow(expr) {          \
            bool _ = false;             \
            try {                       \
                expr;                   \
            } catch (...) {             \
                _ = true;               \
            }                           \
            assert(!_);                 \
        }

class Approx {
public:
    Approx(double val, double eps = 0.001) : val(val), eps(eps) {
        assert(eps > 0);
    }

    double val, eps;
};

inline bool operator==(double val, const Approx &app) {
    return std::abs(val - app.val) < app.eps;
}

inline bool operator==(float val, const Approx &app) {
    return std::abs(val - app.val) < app.eps;
}

#endif