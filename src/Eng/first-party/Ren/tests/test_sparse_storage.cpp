#include "test_common.h"

#include "../utils/SparseStorage.h"

void test_sparse_storage() {
    using namespace Ren;

    printf("Test sparse_storage     | ");

    { // Simple storage
        struct DataMain {
            int light;

            DataMain(int _light) : light(_light) {}
        };

        SparseStorage<DataMain> new_storage;
        require(new_storage.empty());

        const Handle<DataMain, RWTag> handle0 = new_storage.Emplace(0);
        const Handle<DataMain, RWTag> handle1 = new_storage.Emplace(1);
        const Handle<DataMain, RWTag> handle2 = new_storage.Emplace(2);
        const Handle<DataMain, RWTag> handle3 = new_storage.Emplace(3);

        require(new_storage.size() == 4);

        require(handle0.index == 0 && handle0.generation == 0);
        require(handle1.index == 1 && handle1.generation == 0);
        require(handle2.index == 2 && handle2.generation == 0);
        require(handle3.index == 3 && handle3.generation == 0);

        const Handle<void> opaque_handle{handle0};

        [[maybe_unused]] const DataMain &data0 = new_storage[handle0];
        [[maybe_unused]] const DataMain &data1 = new_storage[handle1];
        [[maybe_unused]] const DataMain &data2 = new_storage[handle2];
        [[maybe_unused]] const DataMain &data3 = new_storage[handle3];

        new_storage.Erase(handle0);
        new_storage.Erase(handle1);
        new_storage.Erase(handle2);
        new_storage.Erase(handle3);

        require(new_storage.empty());

        const Handle<DataMain, RWTag> handle4 = new_storage.Emplace(4);
        const Handle<DataMain, RWTag> handle5 = new_storage.Emplace(5);
        const Handle<DataMain, RWTag> handle6 = new_storage.Emplace(6);
        const Handle<DataMain, RWTag> handle7 = new_storage.Emplace(7);

        require(new_storage.size() == 4);

        require(handle4.index == 3 && handle4.generation == 1);
        require(handle5.index == 2 && handle5.generation == 1);
        require(handle6.index == 1 && handle6.generation == 1);
        require(handle7.index == 0 && handle7.generation == 1);

        new_storage.Erase(handle4);
        new_storage.Erase(handle5);
        new_storage.Erase(handle6);
        new_storage.Erase(handle7);

        require(new_storage.empty());
    }

    { // Dual storage
        struct DataMain {
            int light;
        };

        struct DataCold {
            int heavy[8];
        };

        SparseDualStorage<DataMain, DataCold> new_storage;
        require(new_storage.empty());

        const Handle<DataMain> handle0 = new_storage.Emplace();
        const Handle<DataMain> handle1 = new_storage.Emplace();
        const Handle<DataMain> handle2 = new_storage.Emplace();
        const Handle<DataMain> handle3 = new_storage.Emplace();

        require(new_storage.size() == 4);

        require(handle0.index == 0 && handle0.generation == 0);
        require(handle1.index == 1 && handle1.generation == 0);
        require(handle2.index == 2 && handle2.generation == 0);
        require(handle3.index == 3 && handle3.generation == 0);

        const std::pair<DataMain &, DataCold &> data0 = new_storage[handle0];
        const std::pair<DataMain &, DataCold &> data1 = new_storage[handle1];
        const std::pair<DataMain &, DataCold &> data2 = new_storage[handle2];
        const std::pair<DataMain &, DataCold &> data3 = new_storage[handle3];

        data0.first.light = 0;
        data1.first.light = 1;
        data2.first.light = 2;
        data3.first.light = 3;

        new_storage.Erase(handle0);
        new_storage.Erase(handle1);
        new_storage.Erase(handle2);
        new_storage.Erase(handle3);

        require(new_storage.empty());

        const Handle<DataMain> handle4 = new_storage.Emplace();
        const Handle<DataMain> handle5 = new_storage.Emplace();
        const Handle<DataMain> handle6 = new_storage.Emplace();
        const Handle<DataMain> handle7 = new_storage.Emplace();

        require(new_storage.size() == 4);

        require(handle4.index == 3 && handle4.generation == 1);
        require(handle5.index == 2 && handle5.generation == 1);
        require(handle6.index == 1 && handle6.generation == 1);
        require(handle7.index == 0 && handle7.generation == 1);

        new_storage.Erase(handle4);
        new_storage.Erase(handle5);
        new_storage.Erase(handle6);
        new_storage.Erase(handle7);

        require(new_storage.empty());
    }

    { // Handle validity: default handle is false, emplaced handle is true
        Handle<int, RWTag> null_handle;
        require(!null_handle);

        SparseStorage<int> s;
        const auto h = s.Emplace(0);
        require(bool(h));
    }

    { // SparseStorage: Push
        struct Val {
            int v;
        };
        SparseStorage<Val> s;
        Val v1{10}, v2{20};
        const auto h1 = s.Push(v1);
        const auto h2 = s.Push(v2);
        require(s.size() == 2);
        require(s[h1].v == 10);
        require(s[h2].v == 20);
    }

    { // SparseStorage: iterator traversal with holes
        struct Val {
            int v;
            Val(int _v) : v(_v) {}
        };
        SparseStorage<Val> s;
        const auto h0 = s.Emplace(10);
        const auto h1 = s.Emplace(20);
        const auto h2 = s.Emplace(30);
        const auto h3 = s.Emplace(40);

        s.Erase(h1);
        s.Erase(h3);
        require(s.size() == 2);

        int sum = 0, count = 0;
        for (const Val &v : s) {
            sum += v.v;
            ++count;
        }
        require(count == 2);
        require(sum == 40); // 10 + 30

        require(s.begin().handle() == h0);
    }

    { // SparseStorage: Erase(iterator) — erase-while-iterating pattern
        struct Val {
            int v;
            Val(int _v) : v(_v) {}
        };
        SparseStorage<Val> s;
        s.Emplace(1);
        s.Emplace(2);
        s.Emplace(3);
        s.Emplace(4);
        s.Emplace(5);

        for (auto it = s.begin(); it != s.end();) {
            if (it->v % 2 != 0) {
                it = s.Erase(it);
            } else {
                ++it;
            }
        }
        require(s.size() == 2);

        int sum = 0;
        for (const Val &v : s) {
            sum += v.v;
        }
        require(sum == 6); // 2 + 4
    }

    { // SparseStorage: TryGet — valid and stale handle
        struct Val {
            int v;
            Val(int _v) : v(_v) {}
        };
        SparseStorage<Val> s;
        const auto h = s.Emplace(42);

        Val *p = s.TryGet(h);
        require(p != nullptr && p->v == 42);

        s.Erase(h);
        const auto h2 = s.Emplace(99); // reuses same slot
        require(h2.index == h.index && h2.generation == h.generation + 1);

        // h has stale generation — slot is occupied by h2
        require(s.TryGet(h) == nullptr);
        require(s.TryGet(h2) != nullptr && s.TryGet(h2)->v == 99);
    }

    { // SparseStorage: copy and move with non-trivial type
        int alive = 0;
        struct Tracked {
            int *alive_ptr;
            int val;
            Tracked(int *a, int v) : alive_ptr(a), val(v) { ++*a; }
            Tracked(const Tracked &o) : alive_ptr(o.alive_ptr), val(o.val) { ++*alive_ptr; }
            Tracked(Tracked &&o) noexcept : alive_ptr(o.alive_ptr), val(o.val) { ++*alive_ptr; }
            ~Tracked() { --*alive_ptr; }
        };

        {
            SparseStorage<Tracked> s;
            const auto h0 = s.Emplace(&alive, 10);
            const auto h1 = s.Emplace(&alive, 20);
            s.Erase(h0);
            require(alive == 1);

            SparseStorage<Tracked> copy(s);
            require(alive == 2);
            require((copy[Handle<Tracked, RWTag>{h1.index, h1.generation}].val == 20));

            // Modifications to the original must not affect the copy
            s[h1].val = 99;
            require((copy[Handle<Tracked, RWTag>{h1.index, h1.generation}].val == 20));

            // Move: no new objects created or destroyed
            SparseStorage<Tracked> moved(std::move(copy));
            require(alive == 2);
            require(copy.empty());
            require((moved[Handle<Tracked, RWTag>{h1.index, h1.generation}].val == 20));
        }
        require(alive == 0);
    }

    { // SparseStorage: NextOccupied OOB regression — capacity exactly 64
        SparseStorage<int> s;
        Handle<int, RWTag> handles[64];
        for (int i = 0; i < 64; ++i) {
            handles[i] = s.Emplace(i);
        }
        require(s.capacity() == 64);

        int count = 0;
        for (int v : s) {
            (void)v;
            ++count;
        }
        require(count == 64);

        // Explicitly advance past last slot — was the OOB-read case
        auto it = s.iter_at(63);
        ++it;
        require(it == s.end());
    }

    { // SparseDualStorage: Push
        struct Main {
            int v;
        };
        struct Cold {
            float w;
        };
        SparseDualStorage<Main, Cold> s;
        const auto h = s.Push(Main{10}, Cold{3.0f});
        require(s.size() == 1);
        const auto [m, c] = s[h];
        require(m.v == 10 && c.w == 3.0f);
    }

    { // SparseDualStorage: iterator traversal with holes
        struct Main {
            int v;
        };
        struct Cold {
            int w;
        };
        SparseDualStorage<Main, Cold> s;

        auto h0 = s.Emplace();
        s[h0].first.v = 1;
        auto h1 = s.Emplace();
        s[h1].first.v = 2;
        auto h2 = s.Emplace();
        s[h2].first.v = 3;
        s.Erase(h1);

        int sum = 0, count = 0;
        for (auto it = s.begin(); it != s.end(); ++it) {
            sum += (*it).first.v;
            ++count;
        }
        require(count == 2);
        require(sum == 4); // 1 + 3
    }

    { // SparseDualStorage: TryGet — valid and stale handle
        struct Main {
            int v;
        };
        struct Cold {
            int w;
        };
        SparseDualStorage<Main, Cold> s;

        auto h = s.Emplace();
        s[h].first.v = 42;
        s[h].second.w = 7;

        auto [mp, cp] = s.TryGet(h);
        require(mp != nullptr && mp->v == 42);
        require(cp != nullptr && cp->w == 7);

        s.Erase(h);
        const auto h2 = s.Emplace(); // reuses same slot
        require(h2.index == h.index && h2.generation == h.generation + 1);

        auto [stale_m, stale_c] = s.TryGet(h);
        require(stale_m == nullptr && stale_c == nullptr);
    }

    { // SparseDualStorage: copy and move with non-trivial type
        int alive_m = 0, alive_c = 0;
        struct Main {
            int *alive_ptr;
            int val;
            Main(int *a, int v) : alive_ptr(a), val(v) { ++*a; }
            Main(const Main &o) : alive_ptr(o.alive_ptr), val(o.val) { ++*alive_ptr; }
            Main(Main &&o) noexcept : alive_ptr(o.alive_ptr), val(o.val) { ++*alive_ptr; }
            ~Main() { --*alive_ptr; }
        };
        struct Cold {
            int *alive_ptr;
            int val;
            Cold(int *a, int v) : alive_ptr(a), val(v) { ++*a; }
            Cold(const Cold &o) : alive_ptr(o.alive_ptr), val(o.val) { ++*alive_ptr; }
            Cold(Cold &&o) noexcept : alive_ptr(o.alive_ptr), val(o.val) { ++*alive_ptr; }
            ~Cold() { --*alive_ptr; }
        };

        {
            SparseDualStorage<Main, Cold> s;
            const auto h = s.Push(Main(&alive_m, 10), Cold(&alive_c, 20));
            require(alive_m == 1 && alive_c == 1);

            SparseDualStorage<Main, Cold> copy(s);
            require(alive_m == 2 && alive_c == 2);
            {
                auto [m, c] = copy[Handle<Main, RWTag>{h.index, h.generation}];
                require(m.val == 10 && c.val == 20);
            }

            // Move: no new objects created or destroyed
            SparseDualStorage<Main, Cold> moved(std::move(copy));
            require(alive_m == 2 && alive_c == 2);
            require(copy.empty());
        }
        require(alive_m == 0 && alive_c == 0);
    }

    printf("OK\n");
}