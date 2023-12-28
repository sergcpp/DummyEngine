#include "test_common.h"

#include "../ObjectPool.h"
#include "../go/GoAlloc.h"

void test_object_pool() {

    {
        // Test object creation
        struct A {
            int i, j;
            float f;

            A(int _i, int _j, float _f) : i(_i), j(_j), f(_f) {}
        };

        ObjectPool<A> pool(100);

        A *a1 = nullptr, *a2 = nullptr, *a3 = nullptr;

        assert_nothrow(a1 = pool.New(1, 2, 0.5f));
        assert(a1 != nullptr);
        assert(a1->i == 1);
        assert(a1->j == 2);
        assert_nothrow(a2 = pool.New(1, 1, 0.1f));
        assert(a2 != nullptr);
        assert(a2->i == 1);
        assert(a2->j == 1);

        assert_nothrow(pool.Delete(a1));

        assert_nothrow(a3 = pool.New(2, 3, 0.2f));
        assert(a3 != nullptr);
        assert(a3->i == 2);
        assert(a3->j == 3);

        assert(a1 == a3);
    }

    {
        // Test new node allocation
        struct A {
            int i, j;
            double d1, d2;

            explicit A(int _i) : i(_i), j(_i), d1(_i), d2(_i) {}
        };

        ObjectPool<A> pool(2);

        A *a1, *a2, *a3;

        a1 = pool.New(1);
        a2 = pool.New(2);
        a3 = pool.New(3);

        pool.Delete(a1);
        pool.Delete(a2);
        pool.Delete(a3);
    }

    {
        // ObjectPool test
        struct A {
            int i, j;
            double d1, d2;

            explicit A(int _i) : i(_i), j(_i), d1(_i), d2(_i) {}
        };

        A *a1 = GoAlloc<A>::New(1);
        GoAlloc<A>::Delete(a1);

        GoAlloc<A>::Ref a2 = GoAlloc<A>::NewU(2);

        GoRef<A> a3 = GoAlloc<A>::NewU(3);
    }
}

