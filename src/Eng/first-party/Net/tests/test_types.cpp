#include "test_common.h"

#define TEST_BE
#include "../Types.h"

void test_types() {
    printf("Test types              | ");

    // Check endianness conversion

    { // int16
        Net::le_int16 v1 = -11345;
        int16_t v2 = -11345;
        assert(v1 == v2);
        v1 = -11344;
        assert(v1 != v2);
        assert(v1 > v2);
        assert(v2 < v1);
    }
    { // uint16
        Net::le_uint16 v1 = 11345;
        uint16_t v2 = 11345;
        assert(v1 == v2);
        v1 = 11346;
        assert(v1 != v2);
        assert(v1 > v2);
        assert(v2 < v1);
    }
    { // int32
        Net::le_int32 v1 = -113212145;
        int32_t v2 = -113212145;
        assert(v1 == v2);
        v1 = -113212144;
        assert(v1 != v2);
        assert(v1 > v2);
        assert(v2 < v1);
    }
    { // uint32
        Net::le_uint32 v1 = 87815345;
        uint32_t v2 = 87815345;
        assert(v1 == v2);
        v1 = 87815346;
        assert(v1 != v2);
        assert(v1 > v2);
        assert(v2 < v1);
    }
    { // float32
        Net::le_float32 v1 = 15345.015457f;
        float v2 = 15345.015457f;
        assert(v1 == v2);
        v1 = 15345.016457f;
        assert(v1 != v2);
        assert(v1 > v2);
        assert(v2 < v1);
    }
    // Just check if it compiles or not
    /*glm::vec2 v2_1;
    glm::vec3 v3_1;
    Net::le_float32 f1;
    Net::le_vec2 v2_2;
    Net::le_vec3 v3_2;

    v2_2 = v2_2 + v2_2;
    v2_2 += v2_2;

    v2_1 *= f1;
    v2_1 = v2_1 * f1;
    v2_1 /= f1;
    v2_1 = v2_1 / f1;

    v2_1 = v2_2;
    v2_1 += v2_2;
    v2_1 = v2_1 + v2_2;
    v2_1 -= v2_2;
    v2_1 = v2_1 - v2_2;
    v2_1 *= v2_2;
    v2_1 = v2_1 * v2_2;
    v2_1 /= v2_2;
    v2_1 = v2_1 / v2_2;

    v2_2 *= 1.0f;
    v2_2 = v2_2 * 1.0f;
    v2_2 /= 1.0f;
    v2_2 = v2_2 / 1.0f;

    v2_2 = v2_1;
    v2_2 += v2_1;
    v2_2 = v2_2 + v2_1;
    v2_2 -= v2_1;
    v2_2 = v2_2 - v2_1;
    v2_2 *= v2_1;
    v2_2 = v2_2 * v2_1;
    v2_2 /= v2_1;
    v2_2 = v2_2 / v2_1;
//////////////
    v3_2 = v3_2 + v3_2;
    v3_2 += v3_2;

    v3_1 *= f1;
    v3_1 = v3_1 * f1;
    v3_1 /= f1;
    v3_1 = v3_1 / f1;

    v3_1 = v3_2;
    v3_1 += v3_2;
    v3_1 = v3_1 + v3_2;
    v3_1 -= v3_2;
    v3_1 = v3_1 - v3_2;
    v3_1 *= v3_2;
    v3_1 = v3_1 * v3_2;
    v3_1 /= v3_2;
    v3_1 = v3_1 / v3_2;

    v3_2 *= 1.0f;
    v3_2 = v3_2 * 1.0f;
    v3_2 /= 1.0f;
    v3_2 = v3_2 / 1.0f;

    v3_2 = v3_1;
    v3_2 += v3_1;
    v3_2 = v3_2 + v3_1;
    v3_2 -= v3_1;
    v3_2 = v3_2 - v3_1;
    v3_2 *= v3_1;
    v3_2 = v3_2 * v3_1;
    v3_2 /= v3_1;
    v3_2 = v3_2 / v3_1;

    {   // check hton

    }*/

    printf("OK\n");
}