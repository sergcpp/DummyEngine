#include "Json.h"

//#include <cctype>
//#include <cassert>
#include <cstring>

#include <iostream>
#include <stdexcept>

//#pragma warning(disable : 4996)

bool JsNumber::Read(std::istream &in) {
    char c;
    while (in.read(&c, 1) && isspace(c));
    in.seekg(-1, std::ios::cur);
    return bool(in >> val);
}

void JsNumber::Write(std::ostream &out, JsFlags /*flags*/) const {
    out << val;
}

/////////////////////////////////////////////////////////////////

bool JsString::Read(std::istream &in) {
    char cur, prev = 0;
    while (in.read(&cur, 1) && isspace(cur));
    if (cur != '\"') {
        std::cerr << "JsString::Read(): Expected '\"' instead of " << cur << std::endl;
        return false;
    }
    while (in.read(&cur, 1)) {
        if (cur == '\"' && prev != '\\') break;
        if (prev == '\\') {
            char new_c = 0;
            if (cur == '\"' || cur == '\\' || cur == '/' || cur == '\t' || cur == '\n' || cur == '\r' || cur == '\f' || cur == '\b') {
                new_c = cur;
            }
            if (new_c) {
                val = val.substr(0, val.length() - 1);
                val += new_c;
            }
        }
        val += cur;
        prev = cur;
    }
    return true;
}

void JsString::Write(std::ostream &out, JsFlags /*flags*/) const {
    out << '\"' << val << '\"';
}

/////////////////////////////////////////////////////////////////

JsArray::JsArray(const JsElement *v, size_t num) {
    elements.assign(v, v + num);
}

JsArray::JsArray(const std::initializer_list<JsElement> &l) {
    elements.assign(l);
}

JsArray::JsArray(std::initializer_list<JsElement> &&l) {
    elements.assign(std::move(l));
}

const JsElement &JsArray::at(size_t i) const {
    if (i >= Size()) throw std::out_of_range("Index out of range!");
    return operator[](i);
}

bool JsArray::operator==(const JsArray &rhs) const {
    if (elements.size() != rhs.elements.size()) return false;
    return std::equal(elements.begin(), elements.end(), rhs.elements.begin());
}

bool JsArray::Read(std::istream &in) {
    char c;
    if (!in.read(&c, 1) || c != '[') {
        std::cerr << "JsArray::Read(): Expected '[' instead of " << c << std::endl;
        return false;
    }

    while (in.read(&c, 1)) {
        if (isspace(c)) continue;
        if (c == ']') return true;
        in.seekg(-1, std::ios::cur);

        elements.emplace_back(JS_NULL);
        if (!elements.back().Read(in)) {
            return false;
        }

        while (in.read(&c, 1) && isspace(c));
        if (c != ',') {
            if (c != ']') {
                std::cerr << "JsArray::Read(): Expected ']' instead of " << c << std::endl;
                return false;
            }
            return true;
        }
    }
    return false;
}

void JsArray::Write(std::ostream &out, JsFlags flags) const {
    flags.level++;
    std::string ident_str;
    if (flags.use_new_lines) {
        ident_str += '\n';
    }
    if (flags.ident_levels) {
        for (int i = 0; i < flags.level; i++) {
            ident_str += '\t';
        }
    }
    out << '[';
    for (auto it = elements.cbegin(); it != elements.cend(); ++it) {
        out << ident_str;
        it->Write(out, flags);
        if (it != std::prev(elements.cend(), 1)) {
            out << ", ";
        }
    }
    if (!ident_str.empty()) {
        ident_str = ident_str.substr(0, ident_str.length() - 1);
    }
    out << ident_str << ']';
}

/////////////////////////////////////////////////////////////////

std::pair<std::string, JsElement> &JsObject::operator[](size_t i) {
    auto it = elements.begin();
    std::advance(it, i);
    return *it;
}

JsElement &JsObject::operator[](const std::string &s) {
    for (auto &e : elements) {
        if (e.first == s) {
            return e.second;
        }
    }
    elements.emplace_back(s, JsLiteral{ JS_NULL });
    return elements.back().second;
}

const JsElement &JsObject::at(const std::string &s) const {
    for (const auto &e : elements) {
        if (e.first == s) {
            return e.second;
        }
    }
    throw std::out_of_range(std::string("No such element! \"") + s + "\"");
}

bool JsObject::operator==(const JsObject &rhs) const {
    if (elements.size() != rhs.elements.size()) return false;
    return std::equal(elements.begin(), elements.end(), rhs.elements.begin());
}

bool JsObject::Has(const std::string &s) const {
    for (auto &e : elements) {
        if (e.first == s) {
            return true;
        }
    }
    return false;
}

void JsObject::Push(const std::string &s, const JsElement &el) {
    for (auto &e : elements) {
        if (e.first == s) {
            e.second = el;
            return;
        }
    }
    elements.emplace_back(s, el);
}

bool JsObject::Read(std::istream &in) {
    char c;
    if (!in.read(&c, 1) || c != '{') {
        std::cerr << "JsObject::Read(): Expected '{' instead of " << c << std::endl;
        return false;
    }
    while (in.read(&c, 1)) {
        if (isspace(c)) continue;
        if (c == '}') return true;
        in.seekg(-1, std::ios::cur);

        JsString key;
        if (!key.Read(in)) {
            return false;
        }

        while (in.read(&c, 1) && isspace(c));
        if (c != ':') return false;

        while (in.read(&c, 1) && isspace(c));
        in.seekg(-1, std::ios::cur);

        elements.emplace_back(key.val, JsLiteral{ JS_NULL });
        if (!elements.back().second.Read(in)) {
            return false;
        }

        while (in.read(&c, 1) && isspace(c));
        if (c != ',') {
            return (c == '}');
        }
    }
    return false;
}

void JsObject::Write(std::ostream &out, JsFlags flags) const {
    flags.level++;
    std::string ident_str;
    if (flags.use_new_lines) {
        ident_str += '\n';
    }
    if (flags.ident_levels) {
        for (int i = 0; i < flags.level; i++) {
            ident_str += '\t';
        }
    }
    out << '{';
    for (auto it = elements.cbegin(); it != elements.cend(); ++it) {
        out << ident_str;
        out << '\"' << it->first << "\" : ";
        it->second.Write(out, flags);
        if (it != std::prev(elements.end(), 1)) {
            out << ", ";
        }
    }
    if (!ident_str.empty()) {
        ident_str = ident_str.substr(0, ident_str.length() - 1);
    }
    out << ident_str << '}';
}

/////////////////////////////////////////////////////////////////

bool JsLiteral::Read(std::istream &in) {
    char c;
    while (in.read(&c, 1) && isspace(c));
    in.seekg(-1, std::ios::cur);

    char str[5] {};
    if (!in.read(str, 4)) return false;

    if (strcmp(str, "null") == 0) {
        val = JS_NULL;
        return true;
    } else if (strcmp(str, "true") == 0) {
        val = JS_TRUE;
        return true;
    } else if (strcmp(str, "fals") == 0) {
        if (!in.read(&c, 1) || c != 'e') return false;
        val = JS_FALSE;
        return true;
    }

    std::cerr << "JsLiteral::Read(): null, true or false expected" << std::endl;
    return false;
}

void JsLiteral::Write(std::ostream &out, JsFlags /*flags*/) const {
    if (val == JS_TRUE) {
        out << "true";
    } else if (val == JS_FALSE) {
        out << "false";
    } else if (val == JS_NULL) {
        out << "null";
    }
}

/////////////////////////////////////////////////////////////////

void JsElement::DestroyValue() {
    if (type_ == JS_NUMBER) {
        delete num_;
        num_ = nullptr;
    } else if (type_ == JS_STRING) {
        delete str_;
        str_ = nullptr;
    } else if (type_ == JS_ARRAY) {
        delete arr_;
        arr_ = nullptr;
    } else if (type_ == JS_OBJECT) {
        delete obj_;
        obj_ = nullptr;
    } else if (type_ == JS_LITERAL) {
        delete lit_;
        lit_ = nullptr;
    }
}

JsElement::JsElement(double val) : type_(JS_NUMBER) {
    num_ = new JsNumber(val);
}

JsElement::JsElement(const char *str) : type_(JS_STRING) {
    str_ = new JsString(str);
}

JsElement::JsElement(JsLiteralType lit_type) : type_(JS_LITERAL) {
    lit_ = new JsLiteral(lit_type);
}

JsElement::JsElement(JsType type) : type_(type) {
    if (type_ == JS_NUMBER) {
        num_ = new JsNumber();
    } else if (type_ == JS_STRING) {
        str_ = new JsString();
    } else if (type_ == JS_ARRAY) {
        arr_ = new JsArray();
    } else if (type_ == JS_OBJECT) {
        obj_ = new JsObject();
    } else if (type_ == JS_LITERAL) {
        lit_ = new JsLiteral(JS_NULL);
    }
}

JsElement::JsElement(const JsNumber &rhs) {
    type_ = JS_NUMBER;
    num_ = new JsNumber(rhs);
}

JsElement::JsElement(const JsString &rhs) {
    type_ = JS_STRING;
    str_ = new JsString(rhs);
}

JsElement::JsElement(const JsArray &rhs) {
    type_ = JS_ARRAY;
    arr_ = new JsArray(rhs);
}

JsElement::JsElement(const JsObject &rhs) {
    type_ = JS_OBJECT;
    obj_ = new JsObject(rhs);
}

JsElement::JsElement(const JsLiteral &rhs) {
    type_ = JS_LITERAL;
    lit_ = new JsLiteral(rhs);
}

JsElement::JsElement(const JsElement &rhs) {
    type_ = rhs.type_;
    if (type_ == JS_NUMBER) {
        num_ = new JsNumber(*rhs.num_);
    } else if (type_ == JS_STRING) {
        str_ = new JsString(*rhs.str_);
    } else if (type_ == JS_ARRAY) {
        arr_ = new JsArray(*rhs.arr_);
    } else if (type_ == JS_OBJECT) {
        obj_ = new JsObject(*rhs.obj_);
    } else if (type_ == JS_LITERAL) {
        lit_ = new JsLiteral(*rhs.lit_);
    }
}

JsElement::~JsElement() {
    DestroyValue();
}

JsElement::operator JsNumber &() {
    if (type_ != JS_NUMBER) throw std::runtime_error("Wrong type!");
    return *num_;
}

JsElement::operator JsString &() {
    if (type_ != JS_STRING) throw std::runtime_error("Wrong type!");
    return *str_;
}

JsElement::operator JsArray &() {
    if (type_ != JS_ARRAY) throw std::runtime_error("Wrong type!");
    return *arr_;
}

JsElement::operator JsObject &() {
    if (type_ != JS_OBJECT) throw std::runtime_error("Wrong type!");
    return *obj_;
}

JsElement::operator JsLiteral &() {
    if (type_ != JS_LITERAL) throw std::runtime_error("Wrong type!");
    return *lit_;
}

//

JsElement::operator const JsNumber &() const {
    if (type_ != JS_NUMBER) throw std::runtime_error("Wrong type!");
    return *num_;
}

JsElement::operator const JsString &() const {
    if (type_ != JS_STRING) throw std::runtime_error("Wrong type!");
    return *str_;
}

JsElement::operator const JsArray &() const {
    if (type_ != JS_ARRAY) throw std::runtime_error("Wrong type!");
    return *arr_;
}

JsElement::operator const JsObject &() const {
    if (type_ != JS_OBJECT) throw std::runtime_error("Wrong type!");
    return *obj_;
}

JsElement::operator const JsLiteral &() const {
    if (type_ != JS_LITERAL) throw std::runtime_error("Wrong type!");
    return *lit_;
}

JsElement &JsElement::operator=(JsElement &&rhs) {
    DestroyValue();
    type_ = rhs.type_;
    p_ = rhs.p_;
    rhs.p_ = nullptr;

    return *this;
}

JsElement &JsElement::operator=(const JsElement &rhs) {
    DestroyValue();
    type_ = rhs.type_;
    if (type_ == JS_NUMBER) {
        num_ = new JsNumber(*rhs.num_);
    } else if (type_ == JS_STRING) {
        str_ = new JsString(*rhs.str_);
    } else if (type_ == JS_ARRAY) {
        arr_ = new JsArray(*rhs.arr_);
    } else if (type_ == JS_OBJECT) {
        obj_ = new JsObject(*rhs.obj_);
    } else if (type_ == JS_LITERAL) {
        lit_ = new JsLiteral(*rhs.lit_);
    }
    return *this;
}

bool JsElement::operator==(const JsElement &rhs) const {
    if (type_ != rhs.type_) return false;
    if (type_ == JS_NUMBER) {
        return *num_ == *rhs.num_;
    } else if (type_ == JS_STRING) {
        return *str_ == *rhs.str_;
    } else if (type_ == JS_ARRAY) {
        return *arr_ == *rhs.arr_;
    } else if (type_ == JS_OBJECT) {
        return *obj_ == *rhs.obj_;
    } else if (type_ == JS_LITERAL) {
        return *lit_ == *rhs.lit_;
    }
    return false;
}

bool JsElement::Read(std::istream &in) {
    DestroyValue();
    char c;
    while (in.read(&c, 1) && isspace(c));
    in.seekg(-1, std::ios::cur);
    if (c == '\"') {
        type_ = JS_STRING;
        str_ = new JsString();
        return str_->Read(in);
    } else if (c == '[') {
        type_ = JS_ARRAY;
        arr_ = new JsArray();
        return arr_->Read(in);
    } else if (c == '{') {
        type_ = JS_OBJECT;
        obj_ = new JsObject();
        return obj_->Read(in);
    } else {
        if (isdigit(c) || c == '-') {
            type_ = JS_NUMBER;
            num_ = new JsNumber();
            return num_->Read(in);
        } else {
            type_ = JS_LITERAL;
            lit_ = new JsLiteral(JS_NULL);
            return lit_->Read(in);
        }
    }
}

void JsElement::Write(std::ostream &out, JsFlags flags) const {
    if (type_ == JS_NUMBER) {
        num_->Write(out, flags);
    } else if (type_ == JS_STRING) {
        str_->Write(out, flags);
    } else if (type_ == JS_ARRAY) {
        arr_->Write(out, flags);
    } else if (type_ == JS_OBJECT) {
        obj_->Write(out, flags);
    } else if (type_ == JS_LITERAL) {
        lit_->Write(out, flags);
    }
}
