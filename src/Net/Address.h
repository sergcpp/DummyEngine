#pragma once

#include <string>

namespace Net {
    class Address {
    public:
        Address() {
            address_ = 0;
            port_ = 0;
        }
        Address(unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned short port) {
            address_ = (a << 24) | (b << 16) | (c << 8) | d;
            port_ = port;
        }
        Address(unsigned int address, unsigned short port) {
            address_ = address;
            port_ = port;
        }
        Address(const char *str);
        unsigned int address() const {
            return address_;
        }
        unsigned char a() const {
            return (unsigned char)(address_ >> 24);
        }
        unsigned char b() const {
            return (unsigned char)(address_ >> 16);
        }
        unsigned char c() const {
            return (unsigned char)(address_ >> 8);
        }
        unsigned char d() const {
            return (unsigned char)(address_);
        }
        unsigned short port() const {
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
        unsigned int address_;
        unsigned short port_;
    };
}
