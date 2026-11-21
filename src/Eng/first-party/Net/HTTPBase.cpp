#include "HTTPBase.h"

//const char *ContentType::str_type[] {"text/html", "image/x-icon", "image/png"};
std::vector<Net::ContentType::StrType> Net::ContentType::str_types = {{eType::TextHTML,  "html", "text/html"},
                                                                      {eType::TextCSS,   "css",  "text/css"},
                                                                      {eType::ImageIcon, "ico",  "image/x-icon"},
                                                                      {eType::ImagePNG,  "png",  "image/png"}};

std::string Net::ContentType::TypeString(eType type) {
    if (size_t(type) < str_types.size() && str_types[size_t(type)].t == type) {
        return std::string(str_types[size_t(type)].str);
    }
    for (const auto &st : str_types) {
        if (st.t == type) {
            return std::string(st.str);
        }
    }
    return "";
}

Net::ContentType::eType Net::ContentType::TypeByExt(const char *ext) {
    const std::string_view want = ext ? ext : "";
    for (const auto &st : str_types) {
        if (st.ext == want) {
            return st.t;
        }
    }
    return ContentType::eType::Unknown;
}