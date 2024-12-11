#include "test_common.h"

#include <cstring>

#include <functional>
#include <list>
#include <string>
#include <vector>

#include "../parser/PoolAlloc.h"

void test_pool_alloc() {
    using namespace glslx;

    printf("Test pool_alloc         | ");

    { // Pool alloc usage
        PoolAllocator allocator(4, 255);

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
        MultiPoolAllocator<uint8_t> allocator(32, 512);

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
        MultiPoolAllocator<char> my_alloc(32, 512);

        std::vector<int, MultiPoolAllocator<int>> vec(my_alloc);
        vec.reserve(4);

        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);

        std::list<int, MultiPoolAllocator<int>> list(my_alloc);

        list.push_back(1);
        list.push_back(2);
        list.push_back(3);
        list.push_back(4);

        std::basic_string<char, std::char_traits<char>, MultiPoolAllocator<char>> str(my_alloc);

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