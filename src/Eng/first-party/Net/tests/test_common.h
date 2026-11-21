#pragma once

#include <cstdio>
#include <cstdlib>

#include <atomic>

extern bool g_stop_on_fail;
extern std::atomic_int g_failures;

static bool handle_assert(bool passed, const char *expr, const char *file, long line, bool fatal) {
    if (!passed) {
        printf("Assertion failed %s in %s at line %d\n", expr, file, int(line));
        ++g_failures;
        if (fatal) {
            std::exit(1);
        }
    }
    return passed;
}

#define require(x) handle_assert(x, #x, __FILE__, __LINE__, g_stop_on_fail)
#define require_fatal(x) handle_assert(x, #x, __FILE__, __LINE__, true)
#define require_return(x) handle_assert(x, #x , __FILE__, __LINE__, false); if (!(x)) return

// must not use bare assert(): CI runs Release builds, where it compiles to nothing
#define require_throws(expr)                                                                                           \
    {                                                                                                                  \
        bool _ = false;                                                                                                \
        try {                                                                                                          \
            expr;                                                                                                      \
        } catch (...) {                                                                                                \
            _ = true;                                                                                                  \
        }                                                                                                              \
        handle_assert(_, "throws: " #expr, __FILE__, __LINE__, g_stop_on_fail);                                         \
    }

// must not use bare assert(): CI runs Release builds, where it compiles to nothing
#define require_nothrow(expr)                                                                                          \
    {                                                                                                                  \
        bool _ = false;                                                                                                \
        try {                                                                                                          \
            expr;                                                                                                      \
        } catch (...) {                                                                                                \
            _ = true;                                                                                                  \
        }                                                                                                              \
        handle_assert(!_, "nothrow: " #expr, __FILE__, __LINE__, g_stop_on_fail);                                      \
    }
