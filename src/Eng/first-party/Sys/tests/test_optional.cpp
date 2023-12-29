#include "test_common.h"

#include <cstdio>

#include <utility>

#include "../Optional.h"

void test_optional() {
    {
        // Create Optional
        class MyObj {
            bool b;

          public:
            explicit MyObj(int) : b(true) { }
            MyObj(const MyObj &rhs) {
                b = rhs.b;
            }
            MyObj(MyObj &&rhs) noexcept {
                b = rhs.b;
                rhs.b = false;
            }
            ~MyObj() {
                if (b) {
                }
            }
        };

        Sys::Optional<MyObj> v1;
        require(!v1.initialized());
        Sys::Optional<MyObj> v2(MyObj(11));
        require(v2.initialized());
        v1 = v2;
        require(v1.initialized());
        require(v2.initialized());
        v1 = v1;
        require(v1.initialized());
        v1 = std::move(v2);
        require(v1.initialized());
        require(!v2.initialized());
    }
}
