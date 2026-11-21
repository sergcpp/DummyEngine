#include "HTTPRequest.h"

#include <cstdlib>
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
    return Parse(buf, strlen(buf));
}

bool Net::HTTPRequest::Parse(const char *buf, size_t len) {
    if (!buf || len == 0) {
        return false;
    }

    const char *end = buf + len;
    const char *p = buf;

    // request line
    const char *eol = (const char *) memchr(p, '\n', size_t(end - p));
    const char *line_end = eol ? eol : end;
    if (line_end > p && line_end[-1] == '\r') {
        --line_end;
    }
    {
        const char *q = p;
        const char *sp = (const char *) memchr(q, ' ', size_t(line_end - q));
        if (!sp) {
            return false;
        }
        const size_t mlen = size_t(sp - q);
        if (mlen == 3 && memcmp(q, "GET", 3) == 0) {
            method_.type = eMethodType::GET;
        } else if (mlen == 4 && memcmp(q, "POST", 4) == 0) {
            method_.type = eMethodType::POST;
        } else {
            return false;
        }
        q = sp + 1;
        sp = (const char *) memchr(q, ' ', size_t(line_end - q));
        const char *arg_end = sp ? sp : line_end;
        method_.arg.assign(q, arg_end);
        if (sp) {
            q = sp + 1;
            if (size_t(line_end - q) == 8 && memcmp(q, "HTTP/1.0", 8) == 0) {
                method_.ver = eHTTPVer::_1_0;
            } else if (size_t(line_end - q) == 8 && memcmp(q, "HTTP/1.1", 8) == 0) {
                method_.ver = eHTTPVer::_1_1;
            } else {
                return false;
            }
        } else {
            return false; // a missing version is a malformed request
        }
    }
    p = eol ? eol + 1 : end;

    // header fields: "Name: value\r\n", terminated by an empty line
    bool headers_complete = false;
    while (p < end) {
        eol = (const char *) memchr(p, '\n', size_t(end - p));
        line_end = eol ? eol : end;
        if (line_end > p && line_end[-1] == '\r') {
            --line_end;
        }
        if (line_end == p && eol) {
            headers_complete = true;
            break; // empty line: headers complete
        }
        if (line_end == p) {
            break; // buffer ends without a newline after the last header
        }

        const char *colon = (const char *) memchr(p, ':', size_t(line_end - p));
        if (colon && colon + 1 <= line_end) {
            std::string name(p, colon);
            const char *v = colon + 1;
            if (v < line_end && *v == ' ') {
                ++v;
            }
            std::string val(v, line_end);
            if (name == "Host") {
                host_addr_ = Address(val.c_str());
            } else {
                header_fields_.insert(std::make_pair(std::move(name), std::move(val)));
            }
        }
        // malformed lines are skipped, not fed into the field map
        p = eol ? eol + 1 : end;
    }

    // only accept the request once the terminating empty line is present,
    // so a partial buffer is never reported as complete
    return headers_complete;
}