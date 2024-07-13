#pragma once

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <atomic>

extern bool g_stop_on_fail;
extern std::atomic_bool g_tests_success;

static bool handle_assert(bool passed, const char *assert, const char *file, long line, bool fatal) {
    if (!passed) {
        printf("Assertion failed %s in %s at line %d\n", assert, file, int(line));
        g_tests_success = false;
        if (fatal) {
            exit(-1);
        }
    }
    return passed;
}

#define require(x) handle_assert(x, #x, __FILE__, __LINE__, g_stop_on_fail)
#define require_fatal(x) handle_assert(x, #x, __FILE__, __LINE__, true)
#define require_return(x) handle_assert(x, #x , __FILE__, __LINE__, false); if (!(x)) return

#define require_throws(expr)                                                                                           \
    {                                                                                                                  \
        bool _ = false;                                                                                                \
        try {                                                                                                          \
            expr;                                                                                                      \
        } catch (...) {                                                                                                \
            _ = true;                                                                                                  \
        }                                                                                                              \
        assert(_);                                                                                                     \
    }

#define require_nothrow(expr)                                                                                          \
    {                                                                                                                  \
        bool _ = false;                                                                                                \
        try {                                                                                                          \
            expr;                                                                                                      \
        } catch (...) {                                                                                                \
            _ = true;                                                                                                  \
        }                                                                                                              \
        assert(!_);                                                                                                    \
    }
