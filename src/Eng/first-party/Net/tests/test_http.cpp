#include "test_common.h"

#include <cstring>
#include <vector>

#include "../HTTPBase.h"
#include "../HTTPRequest.h"
#include "../WsConnection.h"

namespace {
static const char pack1[] =
    "GET / HTTP/1.1\r\n"
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
    using namespace Net;

    printf("Test http               | ");

    { // Parse upgrade to websocket
        HTTPRequest req;
        require(req.Parse(pack1));

        require(req.method().type == eMethodType::GET);
        require(req.method().arg == "/");
        require(req.method().ver == eHTTPVer::_1_1);

        require(req.host_addr() == Address(192, 168, 0, 102, 30000));

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
    { // length-checked parse of a buffer that is NOT NUL-terminated
        std::vector<char> buf(pack1, pack1 + sizeof(pack1) - 1); // no terminating NUL
        HTTPRequest req;
        require(req.Parse(buf.data(), buf.size()));
        require(req.method().type == eMethodType::GET);
        require(req.method().arg == "/");
        require(req.field("Upgrade") == "websocket");
    }
    { // a header line without a colon is skipped, the rest still parses
        static const char pack2[] =
            "GET /x HTTP/1.1\r\n"
            "garbage line without a colon\r\n"
            "Keep: yes\r\n"
            "\r\n";
        HTTPRequest req;
        require(req.Parse(pack2));
        require(req.method().arg == "/x");
        require(req.field("Keep") == "yes");
    }
    { // incomplete header block must not report success, and must not parse
        // a partial terminator as a complete one
        const size_t full = strlen(pack1);
        for (size_t cut = full - 3; cut < full; ++cut) {
            HTTPRequest req;
            require(!req.Parse(pack1, cut));
        }
        HTTPRequest req;
        require(req.Parse(pack1, full));
    }
    { // request line with a missing version is rejected
        static const char pack3[] = "GET /x\r\nUpgrade: websocket\r\n\r\n";
        HTTPRequest req;
        require(!req.Parse(pack3));
    }
    { // Content-Type output must be one clean line: no bogus ISO-8859-4,
      // no second Content-Transfer-Encoding line smuggled into the value
        ContentType ct;
        require(ct.str() == "text/html; charset=UTF-8");
        ct.type = ContentType::eType::ImagePNG;
        require(ct.str() == "image/png");
        require(ct.TypeString(ContentType::eType::TextCSS) == "text/css");
        require(ContentType::TypeByExt("png") == ContentType::eType::ImagePNG);
        require(ContentType::TypeByExt("nope") == ContentType::eType::Unknown);
        require(ContentType::TypeByExt(nullptr) == ContentType::eType::Unknown);
    }

    printf("OK\n");
}