#pragma once

#pragma warning(disable : 4996)

#include <cstring>
#include <string>
#include <vector>

namespace Net {
    class HTTPField {
    protected:
        std::string key_;
    public:
        HTTPField(const std::string &key) : key_(key) {}
        const std::string &key() const {
            return key_;
        }
        virtual std::string str() const = 0;
    };
    class ContentType : public HTTPField {
    public:
        enum eType { TextHTML = 0, TextCSS, ImageIcon, ImagePNG, Unknown };
        struct StrType {
            eType t;
            char ext[8], str[32];
            StrType(eType t, const char *e, const char *s) : t (t) {
                strcpy(ext, e); strcpy(str, s);
            }
        };
        enum Charset { UTF_8 };
        eType type;
        Charset charset;
        ContentType() : HTTPField("Content-Type"), type(TextHTML), charset(UTF_8) {}
        virtual std::string str() const {
            std::string ret;
            ret += TypeString(type) + "; ";
            if (type == TextHTML && charset == UTF_8) {
                ret += "charset=UTF-8";
            } else {
                ret += "charset=ISO-8859-4\r\n";
                ret += "Content-Transfer-Encoding: binary;";
            }
            return ret;
        }

        static std::vector<StrType> str_types;
        static std::string TypeString(eType type);
        static eType TypeByExt(const char *ext);
    };
    class ContentLen : public HTTPField {
    public:
        size_t len;
        ContentLen() : HTTPField("Content-Length"), len(0) {}
        virtual std::string str() const {
            std::string ret;
            ret += std::to_string(len);
            return ret;
        }
    };
    class MessageBody : public HTTPField {
    public:
        std::string val;
        MessageBody() : HTTPField("") {}
        virtual std::string str() const {
            if (val.empty()) {
                return "";
            }
            return val + "\r\n";
        }
    };
    class SimpleField : public HTTPField {
    public:
        std::string val;
        SimpleField(const std::string &key, const std::string &val) : HTTPField(key), val(val) {}
        virtual std::string str() const {
            return val;
        }
    };
    class HTTPBase {
    protected:
        ContentType content_type_;
        ContentLen content_len_;
        MessageBody body_;
    public:
        HTTPBase() {

        }

        void set_body(const std::string &body) {
            body_.val = body;
        }

        void set_len(size_t len) {
            content_len_.len = len;
        }

        void set_type(ContentType::eType type) {
            content_type_.type = type;
        }
    };
}
