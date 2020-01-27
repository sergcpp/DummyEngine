#pragma once

#include <map>
#include <string>

#include "Address.h"

namespace Net {
    enum MethodType { GET, POST };
    enum HTTPVer {_1_0, _1_1};
    struct Method {
        MethodType type;
        std::string arg;
        HTTPVer ver;
    };

    class HTTPRequest {
        Method      method_;
        Address     host_addr_;

        std::map<std::string, std::string> header_fields_;
    public:
        Method method() const {
            return method_;
        }
        Address host_addr() const {
            return host_addr_;
        }

        std::string field(const std::string &name) const;

        bool Parse(const char *buf);
    };
}
