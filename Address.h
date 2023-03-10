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

        Address(const char *str);

        uint32_t address() const {
            return address_;
        }

        uint8_t a() const {
            return (unsigned char) (address_ >> 24);
        }

        uint8_t b() const {
            return (unsigned char) (address_ >> 16);
        }

        uint8_t c() const {
            return (unsigned char) (address_ >> 8);
        }

        uint8_t d() const {
            return (unsigned char) (address_);
        }

        uint16_t port() const {
            return port_;
        }

        std::string str() const;

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
