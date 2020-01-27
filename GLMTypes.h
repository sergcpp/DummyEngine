#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Types.h"

#if _ENDIANNESS_ == LITTLE_ENDIAN && !defined(TEST_BE)
namespace Net {
    typedef glm::vec2 le_vec2;
    typedef glm::vec3 le_vec3;
}
#elif _ENDIANNESS_ == BIG_ENDIAN || defined(TEST_BE)
namespace Net {
    typedef glm::tvec2<le_float32> le_vec2;
    typedef glm::tvec3<le_float32> le_vec3;

    inline le_vec2 operator+(const glm::vec2 &v1, const le_vec2 &v2) {
        return le_vec2(v1[0] + v2[0], v1[1] + v2[1]);
    }

    inline le_vec2 operator+(const le_vec2 &v2, const glm::vec2 &v1) {
        return le_vec2(v1[0] + v2[0], v1[1] + v2[1]);
    }

    inline le_vec2 operator-(const glm::vec2 &v1, const le_vec2 &v2) {
        return le_vec2(v1[0] - v2[0], v1[1] - v2[1]);
    }

    inline le_vec2 operator-(const le_vec2 &v2, const glm::vec2 &v1) {
        return le_vec2(v1[0] - v2[0], v1[1] - v2[1]);
    }

    inline le_vec2 operator*(const glm::vec2 &v1, const le_vec2 &v2) {
        return le_vec2(v1[0] * v2[0], v1[1] * v2[1]);
    }

    inline le_vec2 operator*(const le_vec2 &v2, const glm::vec2 &v1) {
        return le_vec2(v1[0] * v2[0], v1[1] * v2[1]);
    }

    inline le_vec2 operator*(const le_vec2 &v, const float &f) {
        return v * (le_float32)f;
    }

    inline le_vec2 operator/(const glm::vec2 &v1, const le_vec2 &v2) {
        return le_vec2(v1[0] / v2[0], v1[1] / v2[1]);
    }

    inline le_vec2 operator/(const le_vec2 &v2, const glm::vec2 &v1) {
        return le_vec2(v1[0] / v2[0], v1[1] / v2[1]);
    }

    inline le_vec2 operator/(const le_vec2 &v, const float &f) {
        return v / (le_float32)f;
    }
////////////////////////////////////////
    inline le_vec3 operator+(const glm::vec3 &v1, const le_vec3 &v2) {
        return le_vec3(v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2]);
    }

    inline le_vec3 operator+(const le_vec3 &v2, const glm::vec3 &v1) {
        return le_vec3(v1[0] + v2[0], v1[1] + v2[1], v1[2] + v2[2]);
    }

    inline le_vec3 operator-(const glm::vec3 &v1, const le_vec3 &v2) {
        return le_vec3(v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2]);
    }

    inline le_vec3 operator-(const le_vec3 &v2, const glm::vec3 &v1) {
        return le_vec3(v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2]);
    }

    inline le_vec3 operator*(const glm::vec3 &v1, const le_vec3 &v2) {
        return le_vec3(v1[0] * v2[0], v1[1] * v2[1], v1[2] * v2[2]);
    }

    inline le_vec3 operator*(const le_vec3 &v2, const glm::vec3 &v1) {
        return le_vec3(v1[0] * v2[0], v1[1] * v2[1], v1[2] * v2[2]);
    }

    inline le_vec3 operator*(const le_vec3 &v, const float &f) {
        return v * (le_float32)f;
    }

    inline le_vec3 operator/(const glm::vec3 &v1, const le_vec3 &v2) {
        return le_vec3(v1[0] / v2[0], v1[1] / v2[1], v1[2] / v2[2]);
    }

    inline le_vec3 operator/(const le_vec3 &v2, const glm::vec3 &v1) {
        return le_vec3(v1[0] / v2[0], v1[1] / v2[1], v1[2] / v2[2]);
    }

    inline le_vec3 operator/(const le_vec3 &v, const float &f) {
        return v / (le_float32)f;
    }
}
#endif

