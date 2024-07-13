#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#undef NDEBUG
#include <cassert>
#include <cstdio>

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

#endif