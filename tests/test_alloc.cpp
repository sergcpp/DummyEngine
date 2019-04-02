#include "test_common.h"

#include <functional>
#include <vector>
#include <list>

#include "../BinaryTree.h"
#include "../MonoAlloc.h"

void test_alloc() {
    {   // Basic usage
        char buf[256];
        for (int i = 0; i < 256; i++) {
            buf[i] = (char)i;
        }
        Sys::MonoAlloc<char> my_alloc(buf, sizeof(buf));

        char *p1 = my_alloc.allocate(64);
        require(p1 != nullptr);
        require(std::memcmp(buf, p1, 64) == 0);

        char *p2 = my_alloc.allocate(32);
        require(p2 != nullptr);
        require(std::memcmp(buf + 64, p2, 32) == 0);

        char *p3 = my_alloc.allocate(16);
        require(p3 != nullptr);
        require(std::memcmp(buf + 64 + 32, p3, 16) == 0);

        require_throws(my_alloc.allocate(512 - 64 - 32 - 16 + 1));
    }

    {   // Usage with stl

        char buf[256];
        Sys::MonoAlloc<char> my_alloc(buf, sizeof(buf));

        std::vector<int, Sys::MonoAlloc<int>> vec(my_alloc);
        vec.reserve(4);

        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);

        require(vec[0] == 1);
        require(vec[1] == 2);
        require(vec[2] == 3);
        require(vec[3] == 4);

        std::list<int, Sys::MonoAlloc<int>> list(my_alloc);

        list.push_back(1);
        list.push_back(2);
        list.push_back(3);
        list.push_back(4);

        require(vec[0] == 1);
        require(vec[1] == 2);
        require(vec[2] == 3);
        require(vec[3] == 4);

        auto el = list.begin();
        require(*el == 1);
        el++;
        require(*el == 2);
        el++;
        require(*el == 3);
        el++;
        require(*el == 4);
    }

    {   // Usage with binary tree
        char buf[16 * 1024];
        Sys::MonoAlloc<char> my_alloc(buf, sizeof(buf));

        Sys::BinaryTree<int, std::less<int>, Sys::MonoAlloc<int>> tree(std::less<int>(), my_alloc);

        for (int i = 0; i < 100; i++) {
            tree.push(i);
            if (i) {
                tree.emplace(-i);
            }
        }

        require(tree.size() == 199);

        int expected_max = 99;

        while (!tree.empty()) {
            int max_val;
            tree.extract_top(max_val);
            require(max_val == expected_max);
            expected_max--;
        }

        require(tree.size() == 0);
        require(tree.empty());
    }
}