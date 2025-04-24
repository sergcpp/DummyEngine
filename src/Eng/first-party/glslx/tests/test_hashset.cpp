#include "test_common.h"

#include <vector>

#include "../HashSet32.h"

void test_hashset() {
    using namespace glslx;

    printf("Test hashset            | ");

    { // Basic test
        HashSet32<int> cont;

        for (int i = 0; i < 100; i++) {
            require(cont.Insert(12));
            require(cont.Insert(42));
            require(cont.Insert(15));
            require(cont.Insert(10));
            require(cont.Insert(-42));
            require(cont.Insert(144));

            require(!cont.Insert(12));
            require(!cont.Insert(42));
            require(!cont.Insert(15));
            require(!cont.Insert(10));
            require(!cont.Insert(-42));
            require(!cont.Insert(144));

            int *p_val = nullptr;

            p_val = cont.Find(12);
            require(p_val && *p_val == 12);

            p_val = cont.Find(12);
            require(p_val && *p_val == 12);

            p_val = cont.Find(42);
            require(p_val && *p_val == 42);

            p_val = cont.Find(15);
            require(p_val && *p_val == 15);

            p_val = cont.Find(10);
            require(p_val && *p_val == 10);

            p_val = cont.Find(-42);
            require(p_val && *p_val == -42);

            p_val = cont.Find(144);
            require(p_val && *p_val == 144);

            require(cont.Erase(12));
            require(cont.Erase(42));
            require(cont.Erase(15));
            require(cont.Erase(10));
            require(cont.Erase(-42));
            require(cont.Erase(144));
        }
    }

    { // Test with reallocation
        HashSet32<std::string> cont(16);

        for (int i = 0; i < 100000; i++) {
            std::string key = std::to_string(i);
            cont.Insert(std::move(key));
        }

        require(cont.size() == 100000);

        for (int i = 0; i < 100000; i++) {
            std::string key = std::to_string(i);
            std::string *_key = cont.Find(key);
            require(_key && *_key == key);
            require(cont.Erase(key));
        }

        require(cont.size() == 0);
    }

    { // Test iteration
        HashSet32<std::string> cont(16);

        for (int i = 0; i < 100000; i++) {
            std::string key = std::to_string(i);
            cont.Insert(std::move(key));
        }

        require(cont.size() == 100000);

        { // const iterator
            int values_count = 0;
            for (auto it = cont.cbegin(); it != cont.cend(); ++it) {
                //require(it->key == std::to_string(it->val));
                values_count++;
            }

            require(values_count == 100000);
        }

        { // normal iterator
            int values_count = 0;
            for (auto it = cont.begin(); it != cont.end(); ++it) {
                //require(it->key == std::to_string(it->val));
                values_count++;
            }

            require(values_count == 100000);
        }
    }

    printf("OK\n");
}