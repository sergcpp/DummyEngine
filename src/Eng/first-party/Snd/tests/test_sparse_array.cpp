#include "test_common.h"

#include <algorithm>
#include <random>
#include <vector>

#include "../SparseArray.h"

namespace {
std::vector<int> GenTestData(int size) {
    std::vector<int> vec(size);
    for (int i = 0; i < size; i++) {
        vec[i] = i;
    }
    return vec;
}

int g_tracker_live = 0;
struct Tracker {
    int val;
    explicit Tracker(int v = 0) : val(v) { ++g_tracker_live; }
    Tracker(const Tracker &o) : val(o.val) { ++g_tracker_live; }
    Tracker(Tracker &&o) noexcept : val(o.val) { o.val = -1; ++g_tracker_live; }
    ~Tracker() { --g_tracker_live; }
    Tracker &operator=(const Tracker &o) { val = o.val; return *this; }
};
} // namespace

void test_sparse_array() {
    using namespace Snd;

    printf("Test sparse_array       | ");

    { // reserve method
        SparseArray<int> s1;
        require(s1.size() == 0);
        s1.Reserve(128);
        require(s1.size() == 0);
        require(s1.capacity() == 128);

        auto s1_copy = s1;
        require(s1_copy.size() == 0);
        s1_copy.Reserve(128);
        require(s1_copy.size() == 0);
        require(s1_copy.capacity() == 128);

        auto s1_stolen = std::move(s1);
        require(s1.empty());
        require(s1_stolen.size() == 0);
        s1_stolen.Reserve(128);
        require(s1_stolen.size() == 0);
        require(s1_stolen.capacity() == 128);
    }

    { // pushing elements into array
        SparseArray<int> s1;
        uint32_t i1 = s1.Emplace(1);
        uint32_t i2 = s1.Push(12);
        uint32_t i3 = s1.Push(45);

        auto s1_copy = s1;

        require(i1 == 0);
        require(i2 == 1);
        require(i3 == 2);

        require(s1.at(0) == 1);
        require(s1.at(1) == 12);
        require(s1[2] == 45);

        require(s1_copy.at(0) == 1);
        require(s1_copy.at(1) == 12);
        require(s1_copy[2] == 45);

        auto s1_stolen = std::move(s1_copy);
        require(s1_copy.empty());

        require(s1_stolen.at(0) == 1);
        require(s1_stolen.at(1) == 12);
        require(s1_stolen[2] == 45);

        s1.Erase(1);
        s1_stolen.Erase(1);

        require(s1.at(0) == 1);
        require(s1[2] == 45);
        require(s1_stolen.at(0) == 1);
        require(s1_stolen[2] == 45);

        uint32_t i4 = s1.Push(32);
        uint32_t i5 = s1.Push(78);

        require(i4 == 1);
        require(i5 == 3);

        require(s1.at(0) == 1);
        require(s1.at(1) == 32);
        require(s1[3] == 78);

        auto it = s1.begin();
        require(*it == 1);
        ++it;
        require(*it == 32);

        { // check operator move
            s1_stolen = std::move(s1);
            require(s1.empty());

            require(s1_stolen.at(0) == 1);
            require(s1_stolen.at(1) == 32);
            require(s1_stolen[3] == 78);

            auto it2 = s1_stolen.begin();
            require(*it2 == 1);
            ++it2;
            require(*it2 == 32);
        }
    }

    { // iteration test
        std::vector<int> data = GenTestData(1000);
        SparseArray<int> s1;
        for (int v : data) {
            s1.Push(v);
        }

        auto it = s1.begin();
        for (int i = 0; i < 1000; i++) {
            require(*it == data[i]);
            ++it;
        }

        std::vector<uint32_t> to_delete;
        for (uint32_t i = 0; i < 1000; i += 2) {
            to_delete.push_back(i);
        }

        // make deletion to happen in random order
        std::shuffle(to_delete.begin(), to_delete.end(), std::default_random_engine(0));

        for (uint32_t i : to_delete) {
            s1.Erase(i);
        }

        it = s1.begin();
        for (int i = 1; i < 1000; i += 2) {
            require(*it == data[i]);
            ++it;
        }

        // fill the gaps and make it reallocate
        for (int v : data) {
            for (int i = 0; i < 100; i++) {
                s1.Push(v);
            }
        }

        // check again
        for (int i = 1; i < 1000; i += 2) {
            require(s1[i] == data[i]);
        }
    }

    { // non-trivial type: constructor/destructor symmetry
        g_tracker_live = 0;
        {
            SparseArray<Tracker> s;
            s.Emplace(10); // index 0
            s.Emplace(20); // index 1
            s.Emplace(30); // index 2
            require(g_tracker_live == 3);
            require(s.at(0).val == 10);
            require(s.at(1).val == 20);
            require(s.at(2).val == 30);

            s.Erase(1);
            require(g_tracker_live == 2);

            auto s_copy = s;
            require(g_tracker_live == 4);
            require(s_copy.at(0).val == 10);
            require(s_copy.at(2).val == 30);

            s_copy.Clear();
            require(g_tracker_live == 2);
            require(s_copy.empty());

            s_copy.Emplace(99);
            require(g_tracker_live == 3);
        }
        require(g_tracker_live == 0);
    }

    { // non-trivial type: reallocation moves correctly
        g_tracker_live = 0;
        {
            SparseArray<Tracker> s;
            for (int i = 0; i < 65; ++i) {
                s.Emplace(i); // forces realloc at 65th insert (64 -> 128)
            }
            require(g_tracker_live == 65);
            for (int i = 0; i < 65; ++i) {
                require(s.at(i).val == i);
            }
        }
        require(g_tracker_live == 0);
    }

    { // GetOrNull
        SparseArray<int> s;
        s.Push(10); // index 0
        s.Push(20); // index 1
        s.Erase(0);

        require(s.GetOrNull(0) == nullptr);
        require(s.GetOrNull(1) != nullptr);
        require(*s.GetOrNull(1) == 20);
        require(s.GetOrNull(s.capacity()) == nullptr); // out of range

        const auto &cs = s;
        require(cs.GetOrNull(0) == nullptr);
        require(cs.GetOrNull(1) != nullptr);
        require(*cs.GetOrNull(1) == 20);
    }

    { // Clear
        SparseArray<int> s;
        for (int i = 0; i < 10; ++i) {
            s.Push(i);
        }
        const uint32_t cap = s.capacity();
        s.Clear();
        require(s.empty());
        require(s.size() == 0);
        require(s.capacity() == cap); // capacity unchanged after Clear

        uint32_t idx = s.Push(42);
        require(s.at(idx) == 42);
        require(s.size() == 1);
    }

    { // iterator-based Erase and iter_at
        SparseArray<int> s;
        for (int i = 0; i < 5; ++i) {
            s.Push(i); // indices 0..4
        }

        auto it = s.begin();
        ++it;               // index 1
        it = s.Erase(it);   // erase 1, return iterator to 2
        require(it.index() == 2);
        require(*it == 2);
        require(s.size() == 4);

        // Erase the last occupied element via iterator; result must be end()
        while (true) {
            auto next = it;
            ++next;
            if (next == s.end()) break;
            it = next;
        }
        it = s.Erase(it);
        require(it == s.end());
        require(s.size() == 3);

        // iter_at addresses a specific slot directly
        auto it2 = s.iter_at(0);
        require(it2.index() == 0);
        require(*it2 == 0);
    }

    { // const iteration and citer_at
        SparseArray<int> s;
        for (int i = 0; i < 5; ++i) {
            s.Push(i);
        }

        const auto &cs = s;
        int expected = 0;
        for (const int &v : cs) {
            require(v == expected++);
        }
        require(expected == 5);

        auto cit = cs.citer_at(3);
        require(cit.index() == 3);
        require(*cit == 3);
    }

    { // word-boundary: exactly 64 elements fill one ctrl word
        SparseArray<int> s;
        for (int i = 0; i < 64; ++i) {
            s.Push(i);
        }
        require(s.size() == 64);
        require(s.capacity() == 64);

        int count = 0;
        for (auto it = s.begin(); it != s.end(); ++it) {
            require(*it == count++);
        }
        require(count == 64);

        // Erase the last slot (index 63) — exercises NextOccupied on the last element
        // of a capacity-aligned block (was an OOB bug before the fix)
        s.Erase(63);
        count = 0;
        for (auto it = s.begin(); it != s.end(); ++it) {
            ++count;
        }
        require(count == 63);
    }

    { // FindOccupiedInRange
        SparseArray<int> s;
        for (int i = 0; i < 130; ++i) {
            s.Push(i); // indices 0..129
        }

        // Within one 64-bit word
        require(s.FindOccupiedInRange(5, 10) == 5);
        s.Erase(5);
        s.Erase(6);
        s.Erase(7);
        require(s.FindOccupiedInRange(5, 10) == 8);
        require(s.FindOccupiedInRange(5, 8) == 8); // [5,8) entirely empty, return end

        // Erase all of 0..63
        for (uint32_t i = 0; i < 64; ++i) {
            if (s.GetOrNull(i)) {
                s.Erase(i);
            }
        }
        // Also erase 64 and 65, keeping 66..129
        s.Erase(64);
        s.Erase(65);

        // end exactly on a word boundary: [0,64) is empty, 66..129 are occupied.
        // Old code would read ctrl_[1] & ~0ull and return 66, which is > end=64 (wrong).
        // New code correctly returns end=64 meaning "not found".
        require(s.FindOccupiedInRange(0, 64) == 64);

        // Cross-boundary range: first occupied is 66
        require(s.FindOccupiedInRange(0, 130) == 66);
        require(s.FindOccupiedInRange(63, 68) == 66); // 63..65 gone, 66 present
        require(s.FindOccupiedInRange(66, 130) == 66);

        // Empty range
        require(s.FindOccupiedInRange(10, 10) == 10);
    }

    printf("OK\n");
}
