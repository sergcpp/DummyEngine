#include "test_common.h"

#define TEST_BE
#include "../Types.h"

void test_types() {
    using namespace Net;

    printf("Test types              | ");

    // Check endianness conversion
    { // int16
        le_int16 v1 = -11345;
        int16_t v2 = -11345;
        require(v1 == v2);
        v1 = -11344;
        require(v1 != v2);
        require(v1 > v2);
        require(v2 < v1);
    }
    { // uint16
        le_uint16 v1 = 11345;
        uint16_t v2 = 11345;
        require(v1 == v2);
        v1 = 11346;
        require(v1 != v2);
        require(v1 > v2);
        require(v2 < v1);
    }
    { // int32
        le_int32 v1 = -113212145;
        int32_t v2 = -113212145;
        require(v1 == v2);
        v1 = -113212144;
        require(v1 != v2);
        require(v1 > v2);
        require(v2 < v1);
    }
    { // uint32
        le_uint32 v1 = 87815345;
        uint32_t v2 = 87815345;
        require(v1 == v2);
        v1 = 87815346;
        require(v1 != v2);
        require(v1 > v2);
        require(v2 < v1);
    }
    { // float32
        le_float32 v1 = 15345.015457f;
        float v2 = 15345.015457f;
        require(v1 == v2);
        v1 = 15345.016457f;
        require(v1 != v2);
        require(v1 > v2);
        require(v2 < v1);
    }
    // Just check if it compiles or not
    /*glm::vec2 v2_1;
    glm::vec3 v3_1;
    le_float32 f1;
    le_vec2 v2_2;
    le_vec3 v3_2;

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