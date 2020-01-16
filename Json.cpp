#include "Json.h"

#include <cstring>

#include <iostream>
#include <stdexcept>

bool JsLiteral::Read(std::istream &in) {
    char c;
    while (in.read(&c, 1) && isspace(c));
    in.seekg(-1, std::ios::cur);

    char str[5]{};
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
    elements.assign(l);
}

JsElement &JsArray::operator[](size_t i) {
    auto it = elements.begin();
    std::advance(it, i);
    return *it;
}

const JsElement &JsArray::operator[](size_t i) const {
    auto it = elements.cbegin();
    std::advance(it, i);
    return *it;
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
            ident_str += flags.use_spaces ? "    " : "\t";
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
        ident_str = ident_str.substr(0, ident_str.length() - (flags.use_spaces ? 4 : 1));
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

JsElement &JsObject::at(const std::string &s) {
    for (auto &e : elements) {
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

void JsObject::Push(const std::string &s, JsElement &&el) {
    for (auto &e : elements) {
        if (e.first == s) {
            e.second = std::move(el);
            return;
        }
    }
    elements.emplace_back(s, std::move(el));
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
            ident_str += flags.use_spaces ? "    " : "\t";
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
        ident_str = ident_str.substr(0, ident_str.length() - (flags.use_spaces ? 4 : 1));
    }
    out << ident_str << '}';
}

/////////////////////////////////////////////////////////////////

JsElement::JsElement(JsLiteralType lit_type) : type_(JS_LITERAL) {
    new(&data_) JsLiteral{ lit_type };
}

JsElement::JsElement(double val) : type_(JS_NUMBER) {
    new(&data_) JsNumber{ val };
}

JsElement::JsElement(const char *str) : type_(JS_STRING) {
    new(&data_) JsString{ str };
}

JsElement::JsElement(JsType type) : type_(type) {
    if (type_ == JS_LITERAL) {
        new (&data_) JsLiteral{ JS_NULL };
    } else if (type_ == JS_NUMBER) {
        new (&data_) JsNumber{};
    } else if (type_ == JS_STRING) {
        new (&data_) JsString{};
    } else if (type_ == JS_ARRAY) {
        new (&data_) JsArray{};
    } else if (type_ == JS_OBJECT) {
        new (&data_) JsObject{};
    }
}

JsElement::JsElement(const JsLiteral &rhs) {
    new(&data_) JsLiteral{ rhs };
    type_ = JS_LITERAL;
}

JsElement::JsElement(const JsNumber &rhs) {
    new(&data_) JsNumber{ rhs };
    type_ = JS_NUMBER;
}

JsElement::JsElement(const JsString &rhs) {
    new(&data_) JsString{ rhs };
    type_ = JS_STRING;
}

JsElement::JsElement(const JsArray &rhs) {
    new(&data_) JsArray{ rhs };
    type_ = JS_ARRAY;
}

JsElement::JsElement(const JsObject &rhs) {
    new(&data_) JsObject{ rhs };
    type_ = JS_OBJECT;
}

JsElement::JsElement(const JsElement &rhs) {
    Destroy();
    if (rhs.type_ == JS_LITERAL) {
        new (&data_) JsLiteral{ reinterpret_cast<const JsLiteral &>(rhs.data_) };
    } else if (rhs.type_ == JS_NUMBER) {
        new (&data_) JsNumber{ reinterpret_cast<const JsNumber &>(rhs.data_) };
    } else if (rhs.type_ == JS_STRING) {
        new (&data_) JsString{ reinterpret_cast<const JsString &>(rhs.data_) };
    } else if (rhs.type_ == JS_ARRAY) {
        new (&data_) JsArray{ reinterpret_cast<const JsArray &>(rhs.data_) };
    } else if (rhs.type_ == JS_OBJECT) {
        new (&data_) JsObject{ reinterpret_cast<const JsObject &>(rhs.data_) };
    }
    type_ = rhs.type_;
}

JsElement::~JsElement() {
    Destroy();
}

void JsElement::Destroy() {
    if (type_ == JS_LITERAL) {
        reinterpret_cast<JsLiteral &>(data_).~JsLiteral();
    } else if (type_ == JS_NUMBER) {
        reinterpret_cast<JsNumber &>(data_).~JsNumber();
    } else if (type_ == JS_STRING) {
        reinterpret_cast<JsString &>(data_).~JsString();
    } else if (type_ == JS_ARRAY) {
        reinterpret_cast<JsArray &>(data_).~JsArray();
    } else if (type_ == JS_OBJECT) {
        reinterpret_cast<JsObject &>(data_).~JsObject();
    }
    type_ = JS_INVALID;
}

JsElement::operator JsNumber &() {
    if (type_ != JS_NUMBER) throw std::bad_cast();
    return reinterpret_cast<JsNumber &>(data_);
}

JsElement::operator JsString &() {
    if (type_ != JS_STRING) throw std::bad_cast();
    return reinterpret_cast<JsString &>(data_);
}

JsElement::operator JsArray &() {
    if (type_ != JS_ARRAY) throw std::bad_cast();
    return reinterpret_cast<JsArray &>(data_);
}

JsElement::operator JsObject &() {
    if (type_ != JS_OBJECT) throw std::bad_cast();
    return reinterpret_cast<JsObject &>(data_);
}

JsElement::operator JsLiteral &() {
    if (type_ != JS_LITERAL) throw std::bad_cast();
    return reinterpret_cast<JsLiteral &>(data_);
}

//

JsElement::operator const JsNumber &() const {
    if (type_ != JS_NUMBER) throw std::bad_cast();
    return reinterpret_cast<const JsNumber &>(data_);
}

JsElement::operator const JsString &() const {
    if (type_ != JS_STRING) throw std::bad_cast();
    return reinterpret_cast<const JsString &>(data_);
}

JsElement::operator const JsArray &() const {
    if (type_ != JS_ARRAY) throw std::bad_cast();
    return reinterpret_cast<const JsArray &>(data_);
}

JsElement::operator const JsObject &() const {
    if (type_ != JS_OBJECT) throw std::bad_cast();
    return reinterpret_cast<const JsObject &>(data_);
}

JsElement::operator const JsLiteral &() const {
    if (type_ != JS_LITERAL) throw std::bad_cast();
    return reinterpret_cast<const JsLiteral &>(data_);
}

JsElement &JsElement::operator=(JsElement &&rhs) noexcept {
    Destroy();
    if (rhs.type_ == JS_LITERAL) {
        new (&data_) JsLiteral{ std::move(reinterpret_cast<const JsLiteral &>(rhs.data_)) };
    } else if (rhs.type_ == JS_NUMBER) {
        new (&data_) JsNumber{ std::move(reinterpret_cast<const JsNumber &>(rhs.data_)) };
    } else if (rhs.type_ == JS_STRING) {
        new (&data_) JsString{ std::move(reinterpret_cast<const JsString &>(rhs.data_)) };
    } else if (rhs.type_ == JS_ARRAY) {
        new (&data_) JsArray{ std::move(reinterpret_cast<const JsArray &>(rhs.data_)) };
    } else if (rhs.type_ == JS_OBJECT) {
        new (&data_) JsObject{ std::move(reinterpret_cast<const JsObject &>(rhs.data_)) };
    }
    type_ = rhs.type_;
    return *this;
}

JsElement &JsElement::operator=(const JsElement &rhs) {
    Destroy();
    if (rhs.type_ == JS_LITERAL) {
        new (&data_) JsLiteral{ reinterpret_cast<const JsLiteral &>(rhs.data_) };
    } else if (rhs.type_ == JS_NUMBER) {
        new (&data_) JsNumber{ reinterpret_cast<const JsNumber &>(rhs.data_) };
    } else if (rhs.type_ == JS_STRING) {
        new (&data_) JsString{ reinterpret_cast<const JsString &>(rhs.data_) };
    } else if (rhs.type_ == JS_ARRAY) {
        new (&data_) JsArray{ reinterpret_cast<const JsArray &>(rhs.data_) };
    } else if (rhs.type_ == JS_OBJECT) {
        new (&data_) JsObject{ reinterpret_cast<const JsObject &>(rhs.data_) };
    }
    type_ = rhs.type_;
    return *this;
}

bool JsElement::operator==(const JsElement &rhs) const {
    if (type_ != rhs.type_) return false;

    if (type_ == JS_LITERAL) {
        return reinterpret_cast<const JsLiteral &>(data_) == reinterpret_cast<const JsLiteral &>(rhs.data_);
    } else if (type_ == JS_NUMBER) {
        return reinterpret_cast<const JsNumber &>(data_) == reinterpret_cast<const JsNumber &>(rhs.data_);
    } else if (type_ == JS_STRING) {
        return reinterpret_cast<const JsString &>(data_) == reinterpret_cast<const JsString &>(rhs.data_);
    } else if (type_ == JS_ARRAY) {
        return reinterpret_cast<const JsArray &>(data_) == reinterpret_cast<const JsArray &>(rhs.data_);
    } else if (type_ == JS_OBJECT) {
        return reinterpret_cast<const JsObject &>(data_) == reinterpret_cast<const JsObject &>(rhs.data_);
    }

    return false;
}

bool JsElement::Read(std::istream &in) {
    Destroy();

    char c;
    while (in.read(&c, 1) && isspace(c));
    in.seekg(-1, std::ios::cur);
    if (c == '\"') {
        new (&data_) JsString{};
        type_ = JS_STRING;
        return reinterpret_cast<JsString &>(data_).Read(in);
    } else if (c == '[') {
        new (&data_) JsArray{};
        type_ = JS_ARRAY;
        return reinterpret_cast<JsArray &>(data_).Read(in);
    } else if (c == '{') {
        new (&data_) JsObject{};
        type_ = JS_OBJECT;
        return reinterpret_cast<JsObject &>(data_).Read(in);
    } else {
        if (isdigit(c) || c == '-') {
            new (&data_) JsNumber{};
            type_ = JS_NUMBER;
            return reinterpret_cast<JsNumber &>(data_).Read(in);
        } else {
            new (&data_) JsLiteral{ JS_NULL };
            type_ = JS_LITERAL;
            return reinterpret_cast<JsLiteral &>(data_).Read(in);
        }
    }
}

void JsElement::Write(std::ostream &out, JsFlags flags) const {
    if (type_ == JS_LITERAL) {
        reinterpret_cast<const JsLiteral &>(data_).Write(out, flags);
    } else if (type_ == JS_NUMBER) {
        reinterpret_cast<const JsNumber &>(data_).Write(out, flags);
    } else if (type_ == JS_STRING) {
        reinterpret_cast<const JsString &>(data_).Write(out, flags);
    } else if (type_ == JS_ARRAY) {
        reinterpret_cast<const JsArray &>(data_).Write(out, flags);
    } else if (type_ == JS_OBJECT) {
        reinterpret_cast<const JsObject &>(data_).Write(out, flags);
    }
}
