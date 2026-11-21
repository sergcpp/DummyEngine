#include "test_common.h"

#include "../Address.h"

void test_address() {
    using namespace Net;

    printf("Test address            | ");

    {
        Address addr;
        require(addr.a() == 0);
        require(addr.b() == 0);
        require(addr.c() == 0);
        require(addr.d() == 0);
        require(addr.port() == 0);
        require(addr.address() == 0);
    }
    {
        const unsigned char a = 100;
        const unsigned char b = 110;
        const unsigned char c = 50;
        const unsigned char d = 12;
        const unsigned short port = 10000;
        Address address(a, b, c, d, port);
        require(a == address.a());
        require(b == address.b());
        require(c == address.c());
        require(d == address.d());
        require(port == address.port());
    }
    {
        Address x(100, 110, 0, 1, 50000);
        Address y(101, 210, 6, 5, 50002);
        require(x != y);
        require(y == y);
        require(x == x);
    }
    {
        Address x(100, 110, 0, 1, 50000);
        std::string s = x.str();
        require(s == "100.110.0.1:50000");
    }
    { // address() is host-order: the (unsigned, ushort) ctor takes it verbatim
        const Address x(0x0A000001u, 80);
        require(x.a() == 10 && x.b() == 0 && x.c() == 0 && x.d() == 1);
        require(x == Address(10, 0, 0, 1, 80));
    }

    printf("OK\n");
}