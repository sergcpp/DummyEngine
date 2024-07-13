#include "test_common.h"

#include <cstring>

#include <functional>
#include <list>
#include <string>
#include <vector>

#include "../BinaryTree.h"
#include "../MonoAlloc.h"
#include "../PoolAlloc.h"

void test_alloc() {
    printf("Test alloc              | ");

    { // Basic usage
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

    { // Usage with stl

        char buf[512];
        Sys::MonoAlloc<char> my_alloc(buf, sizeof(buf));

        std::vector<int, Sys::MonoAlloc<int>> vec(my_alloc);
        vec.reserve(4);

        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);

        std::list<int, Sys::MonoAlloc<int>> list(my_alloc);

        list.push_back(1);
        list.push_back(2);
        list.push_back(3);
        list.push_back(4);

        std::basic_string<char, std::char_traits<char>, Sys::MonoAlloc<char>> str(my_alloc);

        str.append("test");
        str.append("string");
        str.append("more");
        str.append("data");
        str.append("to");
        str.append("go");
        str.append("around");
        str.append("small");
        str.append("string");
        str.append("optimization");

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

        require(str == "teststringmoredatatogoaroundsmallstringoptimization");
    }

    { // Usage with binary tree
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

    { // Pool alloc usage
        Sys::PoolAllocator allocator(4, 255);

        int *pointers[512];

        for (int i = 0; i < 512; i++) {
            pointers[i] = (int *)allocator.Alloc();
            *pointers[i] = i;
        }

        for (int i = 0; i < 512; i++) {
            require(*pointers[i] == i);
        }

        for (int i = 0; i < 512; i += 2) {
            allocator.Free(pointers[i]);
        }

        for (int i = 1; i < 512; i += 2) {
            require(*pointers[i] == i);
        }

        for (int i = 1; i < 512; i += 2) {
            allocator.Free(pointers[i]);
        }
    }

    { // Multi-pool alloc
        Sys::MultiPoolAllocator<uint8_t> allocator(32, 512);

        uint8_t *pointers[16][256];

        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 256; j++) {
                pointers[i][j] = allocator.allocate(j + 1);
                for (int k = 0; k < j; k++) {
                    *(pointers[i][j] + k) = (uint8_t)k;
                }
            }
        }

        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 256; j++) {
                for (int k = 0; k < j; k++) {
                    require(*(pointers[i][j] + k) == (uint8_t)k);
                }
            }
        }

        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 256; j += 2) {
                allocator.deallocate(pointers[i][j], j + 1);
            }
        }

        for (int i = 0; i < 16; i++) {
            for (int j = 1; j < 256; j += 2) {
                for (int k = 0; k < j; k++) {
                    require(*(pointers[i][j] + k) == (uint8_t)k);
                }
            }
        }

        for (int i = 0; i < 16; i++) {
            for (int j = 1; j < 256; j += 2) {
                allocator.deallocate(pointers[i][j], j + 1);
            }
        }
    }

    { // Multi-pool alloc stl usage
        Sys::MultiPoolAllocator<char> my_alloc(32, 512);

        std::vector<int, Sys::MultiPoolAllocator<int>> vec(my_alloc);
        vec.reserve(4);

        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);

        std::list<int, Sys::MultiPoolAllocator<int>> list(my_alloc);

        list.push_back(1);
        list.push_back(2);
        list.push_back(3);
        list.push_back(4);

        std::basic_string<char, std::char_traits<char>, Sys::MultiPoolAllocator<char>> str(my_alloc);

        str.append("test");
        str.append("string");
        str.append("more");
        str.append("data");
        str.append("to");
        str.append("go");
        str.append("around");
        str.append("small");
        str.append("string");
        str.append("optimization");

        require(vec[0] == 1);
        require(vec[1] == 2);
        require(vec[2] == 3);
        require(vec[3] == 4);

        auto el = list.begin();
        require(*el++ == 1);
        require(*el++ == 2);
        require(*el++ == 3);
        require(*el == 4);

        require(str == "teststringmoredatatogoaroundsmallstringoptimization");
    }

    printf("OK\n");
}