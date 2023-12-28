#pragma warning(disable : 4018)

#include "HTTPBase.h"

#include <cstring>

//const char *ContentType::str_type[] {"text/html", "image/x-icon", "image/png"};
std::vector<Net::ContentType::StrType> Net::ContentType::str_types = {{eType::TextHTML,  "html", "text/html"},
                                                                      {eType::TextCSS,   "css",  "text/css"},
                                                                      {eType::ImageIcon, "ico",  "image/x-icon"},
                                                                      {eType::ImagePNG,  "png",  "image/png"}};

std::string Net::ContentType::TypeString(eType type) {
    if (size_t(type) < str_types.size() && str_types[int(type)].t == type) {
        return str_types[int(type)].str;
    }
    for (int i = 0; i < str_types.size(); i++) {
        if (str_types[i].t == type) {
            return str_types[i].str;
        }
    }
    return "";
}

Net::ContentType::eType Net::ContentType::TypeByExt(const char *ext) {
    /*if (strcmp(ext, "html") == 0) {
        return TEXT_HTML;
    } else if (strcmp(ext, "ico") == 0) {
        return IMAGE_ICON;
    } else if (strcmp(ext, "png") == 0) {
        return IMAGE_PNG;
    } else {
        return TEXT_HTML;
    }*/
    for (int i = 0; i < str_types.size(); i++) {
        if (strcmp(str_types[i].ext, ext) == 0) {
            return str_types[i].t;
        }
    }
    return ContentType::eType::Unknown;
}