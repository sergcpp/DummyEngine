#pragma warning(disable : 4996)
#include "Address.h"

#include <cstdio>

#ifdef _WIN32
    #include <winsock2.h>
#endif
#if defined(__linux__) || defined(__APPLE__) || defined(EMSCRIPTEN)
    #include <fcntl.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
#endif

Net::Address::Address(const char *str) : address_(0), port_(0) {
    long int pport;
    char hostNameChar[256];
    if (sscanf(str, "%[^:]:%ld", hostNameChar, &pport) == 2) {
        address_ = ntohl(inet_addr(hostNameChar));
        port_ = (unsigned short) pport;
    }
}

std::string Net::Address::str() const {
    char buf[128];
    sprintf(buf, "%i.%i.%i.%i:%i", (int)a(), (int)b(), (int)c(), (int)d(), (int)port());
    return std::string(buf);
}
