#pragma once

#include <cstdint>
#include <cstdlib>

namespace Net {
    class BitMsg {
    protected:
        int write_bit_;
        mutable int read_bit_, read_pos_;
        uint64_t temp_val_;
        uint8_t *write_data_;
        const uint8_t *read_data_;
        size_t len_, cap_;
    public:
        BitMsg(uint8_t *p_data, size_t len);

        BitMsg(const uint8_t *p_data, size_t len);

        int num_bits_written() const { return (int) (len_ * 8 + write_bit_); }

        int num_bits_read() const { return (read_pos_ * 8) - ((8 - read_bit_) & 7); }

        int remaining_write_bits() const { return (int) (cap_ * 8 - num_bits_written()); }

        int remaining_read_bits() const { return (int) (len_ * 8 - num_bits_read()); }

        /*uint8_t *write_data(size_t &size) {
            size = len_;
            return write_data_;
        }*/

        void WriteBits(int val, int num_bits);

        int ReadBits(int num_bits) const;

        template<typename T>
        void Write(T v);

        template<typename T>
        T Read() const;
    };

    template<>
    inline void BitMsg::Write<bool>(bool v) { WriteBits(v, 1); }

    template<>
    inline void BitMsg::Write<int8_t>(int8_t v) { WriteBits(v, -8); }

    template<>
    inline void BitMsg::Write<uint8_t>(uint8_t v) { WriteBits(v, 8); }

    template<>
    inline void BitMsg::Write<int16_t>(int16_t v) { WriteBits(v, -16); }

    template<>
    inline void BitMsg::Write<uint16_t>(uint16_t v) { WriteBits(v, 16); }

    template<>
    inline void BitMsg::Write<int32_t>(int32_t v) { WriteBits(v, 32); }

    template<>
    inline void BitMsg::Write<uint32_t>(uint32_t v) { WriteBits(v, 32); }

    template<>
    inline void BitMsg::Write<int64_t>(int64_t v) {
        int a = (int) v;
        int b = (int) (v >> 32);
        WriteBits(a, 32);
        WriteBits(b, 32);
    }

    template<>
    inline void BitMsg::Write<float>(float v) {
        WriteBits(*reinterpret_cast<int *>(&v), 32);
    }

/////////////////////
    template<>
    inline bool BitMsg::Read<bool>() const { return ReadBits(1) == 1; }

    template<>
    inline int8_t BitMsg::Read<int8_t>() const { return (int8_t) ReadBits(-8); }

    template<>
    inline uint8_t BitMsg::Read<uint8_t>() const { return (uint8_t) ReadBits(8); }

    template<>
    inline int16_t BitMsg::Read<int16_t>() const { return (int16_t) ReadBits(-16); }

    template<>
    inline uint16_t BitMsg::Read<uint16_t>() const { return (uint16_t) ReadBits(16); }

    template<>
    inline int32_t BitMsg::Read<int32_t>() const { return (int32_t) ReadBits(32); }

    template<>
    inline uint32_t BitMsg::Read<uint32_t>() const { return (uint32_t) ReadBits(32); }

    template<>
    inline int64_t BitMsg::Read<int64_t>() const {
        int64_t a = ReadBits(32);
        int64_t b = ReadBits(32);
        int64_t c = (0x00000000ffffffff & a) | (b << 32);
        return c;
    }

    template<>
    inline float BitMsg::Read<float>() const {
        float val;
        *reinterpret_cast<int *>(&val) = ReadBits(32);
        return val;
    }
}
