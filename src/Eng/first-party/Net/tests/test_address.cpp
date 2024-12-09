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

    printf("OK\n");
}