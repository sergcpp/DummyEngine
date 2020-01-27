#pragma once

#include <memory>

#include "HTTPBase.h"

namespace Net {
    class HTTPResponse : public HTTPBase {
        int resp_code_;
        std::string status_line_;
        size_t content_length_;
        std::vector<std::unique_ptr<HTTPField>> fields_;
    public:
        HTTPResponse(int resp_code, const std::string &status_line = "");

        void AddField(std::unique_ptr<HTTPField> &&field);

        std::string str() const;
    };
}
