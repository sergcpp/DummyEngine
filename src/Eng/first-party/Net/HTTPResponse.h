#pragma once

#include <memory>

#include "HTTPBase.h"

namespace Net {
    class HTTPResponse : public HTTPBase {
        int resp_code_;
        std::string status_line_;
        [[maybe_unused]] size_t content_length_;
        std::vector<std::unique_ptr<HTTPField>> fields_;
    public:
        explicit HTTPResponse(int resp_code, std::string status_line = {});

        void AddField(std::unique_ptr<HTTPField> &&field);

        [[nodiscard]] std::string str() const;
    };
}
