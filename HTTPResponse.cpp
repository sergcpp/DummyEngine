#include "HTTPResponse.h"

Net::HTTPResponse::HTTPResponse(int resp_code, const std::string &status_line)
        : resp_code_(resp_code), status_line_(status_line), content_length_(0) {
    if (status_line_.empty() && resp_code_ == 200) {
        status_line_ = "OK";
    }
}

void Net::HTTPResponse::AddField(std::unique_ptr<HTTPField> &&field) {
    fields_.emplace_back(field.release());
}

std::string Net::HTTPResponse::str() const {
    std::string ret;
    ret += "HTTP/1.1 " + std::to_string(resp_code_) + " " + status_line_ + "\r\n";

    for (auto &f : fields_) {
        ret += f->key() + ": " + f->str() + "\r\n";
    }

    if (content_len_.len) {
        ret += content_len_.key() + ":" + content_len_.str() + "\r\n";
        ret += content_type_.key() + ":" + content_type_.str() + "\r\n";
    }
    ret += "\r\n";
    ret += body_.str();
    return ret;
}