#include "HTTPRequest.h"

#include <cstring>
#include <string>

std::string Net::HTTPRequest::field(const std::string &name) const {
    auto it = header_fields_.find(name);
    if (it != header_fields_.end()) {
        return it->second;
    }
    return "";
}

bool Net::HTTPRequest::Parse(const char *buf) {
    if (!strstr(buf, "\r\n\r\n")) {
        return false;
    }

    const char *delims = " \r\n";
    const char *delims2 = "\r\n";
    char const *p = buf;
    char const *q = strpbrk(p + 1, delims);
    for (; p != nullptr && q != nullptr; q = strpbrk(p, delims)) {
        if (p == q) {
            p = q + 1;
            continue;
        }
        std::string item(p, q);

        if (item == "GET") {
            method_.type = eMethodType::GET;
            p = q + 1;
            q = strpbrk(p, delims);
            method_.arg = std::string(p, q);
            p = q + 1;
            q = strpbrk(p, delims);
            item = std::string(p, q);
            if (item == "HTTP/1.0") {
                method_.ver = eHTTPVer::_1_0;
            } else if (item == "HTTP/1.1") {
                method_.ver = eHTTPVer::_1_1;
            }
        } else if (item == "POST") {
            method_.type = eMethodType::POST;
        } else if (item == "Host:") {
            p = q + 1;
            q = strpbrk(p, delims);
            host_addr_ = Address(std::string(p, q).c_str());
        } else {
            if (!item.empty()) {
                item.resize(item.length() - 1);
            }
            p = q + 1;
            q = strpbrk(p, delims2);
            std::string val(p, q);
            header_fields_.insert(std::make_pair(std::move(item), std::move(val)));
        }
        if (!q) break;
        p = q + 1;
    }

    return true;
}