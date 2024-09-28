#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>

#define _ENDIANNESS_ LITTLE_ENDIAN

#pragma warning(disable : 4996)

//#undef TEST_BE

namespace Net {
    template<typename T>
    T swap_endian(T u) {
        static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");
        union {
            T u;
            unsigned char u8[sizeof(T)];
        } source, dest;
        source.u = u;
        for (size_t k = 0; k < sizeof(T); k++) {
            dest.u8[k] = source.u8[sizeof(T) - k - 1];
        }
        return dest.u;
    }
}

#if _ENDIANNESS_ == LITTLE_ENDIAN && !defined(TEST_BE)
namespace Net {
    typedef int16_t le_int16;
    typedef uint16_t le_uint16;
    typedef int32_t le_int32;
    typedef uint32_t le_uint32;
    typedef float le_float32;

    template<typename T>
    T hton(T v) {
        return swap_endian(v);
    }

    template<typename T>
    T ntoh(T v) {
        return swap_endian(v);
    }
}
#elif _ENDIANNESS_ == BIG_ENDIAN || defined(TEST_BE)
namespace Net {
#pragma pack(push,1)
    template<typename T>
    class le_type {
    public:
        le_type() : le_val_(0) { }
        le_type(const T &val) : le_val_(swap_endian<T>(val)) { }
        operator T() const {
            return swap_endian<T>(le_val_);
        }
        le_type<T> operator+=(const le_type<T> &f) {
            return le_type<T>((T)le_val_ + (T)f.le_val_);
        }
        le_type<T> operator-=(const le_type<T> &f) {
            return le_type<T>((T)le_val_ - (T)f.le_val_);
        }
        le_type<T> operator*=(const le_type<T> &f) {
            return le_type<T>((T)le_val_ * (T)f.le_val_);
        }
        le_type<T> operator/=(const le_type<T> &f) {
            return le_type<T>((T)le_val_ / (T)f.le_val_);
        }
        le_type<T>& operator=(const T &f) {
            le_val_ = swap_endian<T>(f);
            return *this;
        }
        /*le_type<T> operator+(const le_type<T> &f1, const le_type<T> &f2) {
            return le_type<T>((T)f1.le_val_ + (T)f2.le_val_);
        }*/
    private:
        T le_val_;
    };
#pragma pack(pop)


    typedef le_type<int16_t>    le_int16;
    typedef le_type<uint16_t>   le_uint16;
    typedef le_type<int32_t>    le_int32;
    typedef le_type<uint32_t>   le_uint32;
    typedef le_type<float>      le_float32;
}
#endif

struct StrParam {
    char str[64];

    StrParam() {
        memset(str, 0, sizeof(str));
    }

    StrParam(const char *s) {
        strcpy(str, s);
    }
};

