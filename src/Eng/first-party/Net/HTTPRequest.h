#pragma once

#include <map>
#include <string>

#include "Address.h"

namespace Net {
    enum class eMethodType {
        GET, POST
    };
    enum class eHTTPVer {
        _1_0, _1_1
    };
    struct Method {
        eMethodType type;
        std::string arg;
        eHTTPVer ver;
    };

    class HTTPRequest {
        Method method_;
        Address host_addr_;

        std::map<std::string, std::string> header_fields_;
    public:
        [[nodiscard]] Method method() const { return method_; }
        [[nodiscard]] Address host_addr() const { return host_addr_; }
        [[nodiscard]] std::string field(const std::string &name) const;

        // the NUL-terminated form copies the buffer length with strlen; a network
        // buffer that is only NUL-terminated by convention must use the (buf, len) form
        bool Parse(const char *buf);

        // parses only [buf, buf + len); requires a complete header block (\r\n\r\n)
        bool Parse(const char *buf, size_t len);
    };
}
