#include "test_common.h"

#include "../Types.h"

void test_hton() {
    using namespace Net;

    printf("Test hton               | ");

    { // uint32_t
        uint32_t v1 = 0x12345678;
        uint32_t v2 = hton(v1);
        require(v2 == 0x78563412);
        v2 = ntoh(v2);
        require(v2 == v1);
    }

    { // uint16_t
        uint16_t v1 = 0x1234;
        uint16_t v2 = hton(v1);
        require(v2 == 0x3412);
        v2 = ntoh(v2);
        require(v2 == v1);
    }

    printf("OK\n");
}