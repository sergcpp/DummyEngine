#include "test_common.h"

#include "../ScopeExit.h"

void test_scope_exit() {
    using namespace Sys;

    printf("Test scope_exit         | ");

    { // basic usage
        int test = 0;
        {
            require(test == 0);
            SCOPE_EXIT(test = 42);
            require(test == 0);
        }
        require(test == 42);
    }
    { // dismiss
        int test = 0;
        {
            require(test == 0);
            auto s = AtScopeExit([&] { test = 42; });
            require(test == 0);
            s.Dismiss();
            require(test == 0);
        }
        require(test == 0);
    }
    { // execute
        int test = 0;
        {
            require(test == 0);
            auto s = AtScopeExit([&] { test += 42; });
            require(test == 0);
            s.Execute();
            require(test == 42);
        }
        require(test == 42);
    }
    { // ???
        struct local_flag {
            bool b = false;
        };
        local_flag lf;
        require(lf.b == false);
        {
            auto _ = AtScopeExit([&] { lf.b = true; });
        }
        require(lf.b);
    }

    printf("OK\n");
}