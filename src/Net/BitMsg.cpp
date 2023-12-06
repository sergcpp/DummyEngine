#include "BitMsg.h"

#include <cassert>
#include <cstring>
#include <stdexcept>

#include <string>

namespace {
#define MASK_FIRST_N(x) (uint64_t) ((1ull << (x)) - 1)
#define MFN(x) MASK_FIRST_N(x)
    uint64_t mask_first_n_bits[33] = {
            MFN(0x00u), MFN(0x01u), MFN(0x02u), MFN(0x03u),
            MFN(0x04u), MFN(0x05u), MFN(0x06u), MFN(0x07u),
            MFN(0x08u), MFN(0x09u), MFN(0x0Au), MFN(0x0Bu),
            MFN(0x0Cu), MFN(0x0Du), MFN(0x0Eu), MFN(0x0Fu),
            MFN(0x10u), MFN(0x11u), MFN(0x12u), MFN(0x13u),
            MFN(0x14u), MFN(0x15u), MFN(0x16u), MFN(0x17u),
            MFN(0x18u), MFN(0x19u), MFN(0x1Au), MFN(0x1Bu),
            MFN(0x1Cu), MFN(0x1Du), MFN(0x1Eu), MFN(0x1Fu), 0xFFFFFFFFu
    };
}

Net::BitMsg::BitMsg(uint8_t *p_data, size_t len)
        : write_bit_(0), read_bit_(0), read_pos_(0), temp_val_(0), write_data_(p_data), read_data_(p_data),
          len_(0), cap_(len) {}

Net::BitMsg::BitMsg(const uint8_t *p_data, size_t len)
        : write_bit_(0), read_bit_(0), read_pos_(0), temp_val_(0), write_data_(nullptr), read_data_(p_data),
          len_(len), cap_(len) {}

void Net::BitMsg::WriteBits(int val, int num_bits) {
    assert(write_data_);

    if (num_bits == 0 || num_bits < -31 || num_bits > 32) {
        throw std::runtime_error(std::string("Wrong number of bits: ") + std::to_string(num_bits));
    }

    if (num_bits != 32) {
        if (num_bits > 0) {
            if (val > (1 << num_bits) - 1) {
                throw std::runtime_error(
                        std::string("Value overflow: ") + std::to_string(val) + " " + std::to_string(num_bits));
            } else if (val < 0) {
                throw std::runtime_error("Use negative number of bits for negative number");
            }
        } else {
            int r = 1 << (-num_bits - 1);
            if (val > r - 1) {
                throw std::runtime_error(
                        std::string("Value overflow: ") + std::to_string(val) + " " + std::to_string(num_bits));
            } else if (val < -r) {
                throw std::runtime_error(
                        std::string("Value overflow: ") + std::to_string(val) + " " + std::to_string(num_bits));
            }
        }
    }

    if (num_bits < 0) {
        num_bits = -num_bits;
    }

    if (num_bits > remaining_write_bits()) {
        throw std::runtime_error("Overflow");
    }

    temp_val_ |= (((uint64_t) val) & mask_first_n_bits[num_bits]) << uint64_t(write_bit_);
    write_bit_ += num_bits;

    while (write_bit_ >= 8) {
        write_data_[len_++] = /*(uint8_t)*/(temp_val_ & 255u);
        temp_val_ >>= 8u;
        write_bit_ -= 8;
    }

    if (write_bit_ > 0) {
        write_data_[len_] = (uint8_t) (temp_val_ & 255u);
    }
}

int Net::BitMsg::ReadBits(int num_bits) const {
    if (num_bits == 0 || num_bits < -31 || num_bits > 32) {
        throw std::runtime_error(std::string("Wrong number of bits: ") + std::to_string(num_bits));
    }
    bool with_sign = false;
    if (num_bits < 0) {
        num_bits = -num_bits;
        with_sign = true;
    }

    if (num_bits > remaining_read_bits()) {
        throw std::runtime_error("Cannot read");
    }

    int val = 0;
    int val_bits = 0, fraction;
    while (val_bits < num_bits) {
        if (read_bit_ == 0) {
            read_pos_++;
        }
        int get = 8 - read_bit_;
        if (get > num_bits - val_bits) {
            get = num_bits - val_bits;
        }
        fraction = read_data_[read_pos_ - 1];
        fraction >>= read_bit_;
        fraction &= (1 << get) - 1;
        val |= fraction << val_bits;

        val_bits += get;
        read_bit_ = (read_bit_ + get) & 7;
    }

    if (with_sign) {
        if (val & (1 << (num_bits - 1))) {
            val |= -1 ^ ((1 << num_bits) - 1);
        }
    }
    return val;
}