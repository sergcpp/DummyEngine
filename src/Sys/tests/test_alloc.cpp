#include "test_common.h"

#include <cstring>

#include <functional>
#include <list>
#include <string>
#include <vector>

#include "../BinaryTree.h"
#include "../BitmapAlloc.h"
#include "../MonoAlloc.h"
#include "../PoolAlloc.h"

void test_alloc() {
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

    { // BitmapAllocator first fit
        Sys::BitmapAllocator alloc(16, 16 * 1024 * 1024);

        void *mem0 = alloc.Alloc_FirstFit(256);
        void *mem1 = alloc.Alloc_FirstFit(64);
        require(uintptr_t(mem1) == uintptr_t(mem0) + 256);
        void *mem2 = alloc.Alloc_FirstFit(128);
        require(uintptr_t(mem2) == uintptr_t(mem1) + 64);
        void *mem3 = alloc.Alloc_FirstFit(48);
        require(uintptr_t(mem3) == uintptr_t(mem2) + 128);
        void *mem4 = alloc.Alloc_FirstFit(128);
        require(uintptr_t(mem4) == uintptr_t(mem3) + 48);
        void *mem5 = alloc.Alloc_FirstFit(24);
        require(uintptr_t(mem5) == uintptr_t(mem4) + 128);
        void *mem6 = alloc.Alloc_FirstFit(128);
        require(uintptr_t(mem6) == uintptr_t(mem5) + 32);
        alloc.Free(mem1, 64);
        alloc.Free(mem3, 48);
        alloc.Free(mem5, 24);

        void *mem7 = alloc.Alloc_FirstFit(24);
        require(uintptr_t(mem7) == uintptr_t(mem1));

        alloc.Free(mem0, 256);
        alloc.Free(mem2, 128);
        alloc.Free(mem4, 128);
        alloc.Free(mem6, 128);
        alloc.Free(mem7, 24);
    }

    { // BitmapAllocator best fit
        Sys::BitmapAllocator alloc(16, 16 * 1024 * 1024);

        void *mem0 = alloc.Alloc_BestFit(256);
        void *mem1 = alloc.Alloc_BestFit(64);
        require(uintptr_t(mem1) == uintptr_t(mem0) + 256);
        void *mem2 = alloc.Alloc_BestFit(128);
        require(uintptr_t(mem2) == uintptr_t(mem1) + 64);
        void *mem3 = alloc.Alloc_BestFit(48);
        require(uintptr_t(mem3) == uintptr_t(mem2) + 128);
        void *mem4 = alloc.Alloc_BestFit(128);
        require(uintptr_t(mem4) == uintptr_t(mem3) + 48);
        void *mem5 = alloc.Alloc_BestFit(24);
        require(uintptr_t(mem5) == uintptr_t(mem4) + 128);
        void *mem6 = alloc.Alloc_BestFit(128);
        require(uintptr_t(mem6) == uintptr_t(mem5) + 32);
        alloc.Free(mem1, 64);
        alloc.Free(mem3, 48);
        alloc.Free(mem5, 24);

        void *mem7 = alloc.Alloc_BestFit(24);
        require(uintptr_t(mem7) == uintptr_t(mem5));

        alloc.Free(mem0, 256);
        alloc.Free(mem2, 128);
        alloc.Free(mem4, 128);
        alloc.Free(mem6, 128);
        alloc.Free(mem7, 24);
    }

    { // BitmapAllocator stress test
        uint8_t cmp_buf[256];
        for (int i = 0; i < 256; ++i) {
            cmp_buf[i] = uint8_t(i);
        }

        volatile int free_space = 128 * 1024 * 1024;
        Sys::BitmapAllocator alloc(1 * 1024, free_space);

        std::vector<std::pair<uint8_t *, int>> allocated;

        for (int i = 0; i < 32 * 1024; ++i) {
            const int size_to_alloc = 1 + rand() % (8 * 1024 * 1024);
            uint8_t *new_alloc;
            if (i % 2) {
                new_alloc = reinterpret_cast<uint8_t *>(alloc.Alloc_FirstFit(size_to_alloc));
            } else {
                new_alloc = reinterpret_cast<uint8_t *>(alloc.Alloc_BestFit(size_to_alloc));
            }
            if (!new_alloc) {
                const int index_to_free = rand() % allocated.size();
                auto it_to_free = begin(allocated) + index_to_free;
                for (int i = 0; i < it_to_free->second; i += 256) {
                    require(memcmp(&it_to_free->first[i], cmp_buf, std::min(it_to_free->second - i, 256)) == 0);
                }
                alloc.Free(it_to_free->first, it_to_free->second);
                free_space += it_to_free->second;
                allocated.erase(it_to_free);
            } else {
                for (int j = 0; j < size_to_alloc; j += 256) {
                    memcpy(&new_alloc[j], cmp_buf, std::min(size_to_alloc - j, 256));
                }
                free_space -= size_to_alloc;
                allocated.emplace_back(new_alloc, size_to_alloc);
            }
        }

        for (const auto &al : allocated) {
            for (int i = 0; i < al.second; i += 256) {
                require(memcmp(&al.first[i], cmp_buf, std::min(al.second - i, 256)) == 0);
            }
            alloc.Free(al.first, al.second);
        }
    }
}