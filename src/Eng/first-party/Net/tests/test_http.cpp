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

    { // Parse upgrade to websocket
        Net::HTTPRequest req;
        assert(req.Parse(pack1));

        assert(req.method().type == Net::eMethodType::GET);
        assert(req.method().arg == "/");
        assert(req.method().ver == Net::eHTTPVer::_1_1);

        assert(req.host_addr() == Net::Address(192, 168, 0, 102, 30000));

        assert(req.field("User-Agent") ==
               "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:48.0) Gecko/20100101 Firefox/48.0");

        assert(req.field("Accept") == "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        assert(req.field("Accept-Language") == "en-US,en;q=0.5");
        assert(req.field("Accept-Encoding") == "gzip, deflate");

        assert(req.field("Sec-WebSocket-Version") == "13");
        assert(req.field("Origin") == "null");
        assert(req.field("Sec-WebSocket-Protocol") == "binary");
        assert(req.field("Sec-WebSocket-Extensions") == "permessage-deflate");

        assert(req.field("Connection") == "keep-alive, Upgrade");
        assert(req.field("Pragma") == "no-cache");
        assert(req.field("Cache-Control") == "no-cache");
        assert(req.field("Upgrade") == "websocket");
    }
}