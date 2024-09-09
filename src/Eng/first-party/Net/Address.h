#pragma once

#include <cstdint>

#include <string>

namespace Net {
    class Address {
    public:
        Address() {
            address_ = 0;
            port_ = 0;
        }

        Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) {
            address_ = (a << 24) | (b << 16) | (c << 8) | d;
            port_ = port;
        }

        Address(unsigned int address, unsigned short port) {
            address_ = address;
            port_ = port;
        }

        explicit Address(const char *str);

        [[nodiscard]] uint32_t address() const { return address_; }

        [[nodiscard]] uint8_t a() const { return uint8_t(address_ >> 24); }
        [[nodiscard]] uint8_t b() const { return uint8_t(address_ >> 16); }
        [[nodiscard]] uint8_t c() const { return uint8_t(address_ >> 8); }
        [[nodiscard]] uint8_t d() const { return uint8_t(address_); }

        [[nodiscard]] uint16_t port() const { return port_; }

        [[nodiscard]] std::string str() const;

        bool operator==(const Address &other) const {
            return address_ == other.address_ && port_ == other.port_;
        }

        bool operator!=(const Address &other) const {
            return !(*this == other);
        }

        bool operator<(const Address &other) const {
            if (address_ < other.address_) {
                return true;
            } else if (address_ > other.address_) {
                return false;
            }
            if (port_ < other.port_) {
                return true;
            } else if (port_ > other.port_) {
                return false;
            }
            return false;
        }

    private:
        uint32_t address_;
        uint16_t port_;
    };
}
