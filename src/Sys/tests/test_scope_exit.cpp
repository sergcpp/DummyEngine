#include "test_common.h"

#include "../ScopeExit.h"

void test_scope_exit() {
    bool test = false;

    {
        SCOPE_EXIT(test = true);
        require(!test);
    }

    require(test);
}