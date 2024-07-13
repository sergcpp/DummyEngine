#include "test_common.h"

#include "../HTTPRequest.h"
#include "../WsConnection.h"

namespace {
const char pack1[] = "GET / HTTP/1.1\r\n"
                     "Host: 192.168.0.102:30000\r\n"
                     "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:48.0) Gecko/20100101 Firefox/48.0\r\n"
                     "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                     "Accept-Language: en-US,en;q=0.5\r\n"
                     "Accept-Encoding: gzip, deflate\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "Origin: null\r\n"
                     "Sec-WebSocket-Protocol: binary\r\n"
                     "Sec-WebSocket-Extensions: permessage-deflate\r\n"
                     //"Sec-WebSocket-Key: rNSWTkPMwVG+y3MD/XMWEA==\r\n"
                     "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                     "Connection: keep-alive, Upgrade\r\n"
                     "Pragma: no-cache\r\n"
                     "Cache-Control: no-cache\n"
                     "Upgrade: websocket\r\n\r\n";

}

void test_http() {
    printf("Test http               | ");

    { // Parse upgrade to websocket
        Net::HTTPRequest req;
        require(req.Parse(pack1));

        require(req.method().type == Net::eMethodType::GET);
        require(req.method().arg == "/");
        require(req.method().ver == Net::eHTTPVer::_1_1);

        require(req.host_addr() == Net::Address(192, 168, 0, 102, 30000));

        require(req.field("User-Agent") ==
                "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:48.0) Gecko/20100101 Firefox/48.0");

        require(req.field("Accept") == "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        require(req.field("Accept-Language") == "en-US,en;q=0.5");
        require(req.field("Accept-Encoding") == "gzip, deflate");

        require(req.field("Sec-WebSocket-Version") == "13");
        require(req.field("Origin") == "null");
        require(req.field("Sec-WebSocket-Protocol") == "binary");
        require(req.field("Sec-WebSocket-Extensions") == "permessage-deflate");

        require(req.field("Connection") == "keep-alive, Upgrade");
        require(req.field("Pragma") == "no-cache");
        require(req.field("Cache-Control") == "no-cache");
        require(req.field("Upgrade") == "websocket");
    }

    printf("OK\n");
}