#include "Json.h"

#include <cstring>

#include <algorithm>
#include <iostream>
#include <stdexcept>

bool JsLiteral::Read(std::istream &in) {
    char c;
    while (in.read(&c, 1) && isspace(c))
        ;
    in.seekg(-1, std::ios::cur);

    char str[5]{};
    if (!in.read(str, 4))
        return false;

    if (strcmp(str, "null") == 0) {
        val = JsLiteralType::Null;
        return true;
    } else if (strcmp(str, "true") == 0) {
        val = JsLiteralType::True;
        return true;
    } else if (strcmp(str, "fals") == 0) {
        if (!in.read(&c, 1) || c != 'e')
            return false;
        val = JsLiteralType::False;
        return true;
    }

    std::cerr << "JsLiteral::Read(): null, true or false expected" << std::endl;
    return false;
}

void JsLiteral::Write(std::ostream &out, const JsFlags /*flags*/) const {
    if (val == JsLiteralType::True) {
        out << "true";
    } else if (val == JsLiteralType::False) {
        out << "false";
    } else if (val == JsLiteralType::Null) {
        out << "null";
    }
}

/////////////////////////////////////////////////////////////////

bool JsNumber::Read(std::istream &in) {
    char c;
    while (in.read(&c, 1) && isspace(c))
        ;
    in.seekg(-1, std::ios::cur);
    return bool(in >> val);
}

void JsNumber::Write(std::ostream &out, const JsFlags /*flags*/) const { out << val; }

/////////////////////////////////////////////////////////////////

template <typename Alloc> bool JsStringT<Alloc>::Read(std::istream &in) {
    char cur, prev = 0;
    while (in.read(&cur, 1) && isspace(cur))
        ;
    if (cur != '\"') {
        std::cerr << "JsString::Read(): Expected '\"' instead of " << cur << std::endl;
        return false;
    }
    while (in.read(&cur, 1)) {
        if (cur == '\"' && prev != '\\')
            break;
        if (prev == '\\') {
            char new_c = 0;
            if (cur == '\"' || cur == '\\' || cur == '/' || cur == '\t' || cur == '\n' || cur == '\r' || cur == '\f' ||
                cur == '\b') {
                new_c = cur;
            }
            if (new_c) {
                val.pop_back();
                val += new_c;
            }
        }
        val += cur;
        prev = cur;
    }
    return true;
}

template <typename Allocator> void JsStringT<Allocator>::Write(std::ostream &out, const JsFlags /*flags*/) const {
    out << '\"' << val << '\"';
}

template struct JsStringT<std::allocator<char>>;
template struct JsStringT<Sys::MultiPoolAllocator<char>>;

/////////////////////////////////////////////////////////////////

template <typename Alloc>
JsArrayT<Alloc>::JsArrayT(const JsElementT<Alloc> *v, const size_t num, const Alloc &alloc) : elements(alloc) {
    elements.assign(v, v + num);
}

template <typename Alloc> JsElementT<Alloc> &JsArrayT<Alloc>::operator[](const size_t i) {
    auto it = elements.begin();
    std::advance(it, i);
    return *it;
}

template <typename Alloc> const JsElementT<Alloc> &JsArrayT<Alloc>::operator[](const size_t i) const {
    auto it = elements.cbegin();
    std::advance(it, i);
    return *it;
}

template <typename Alloc> const JsElementT<Alloc> &JsArrayT<Alloc>::at(const size_t i) const {
    if (i >= Size()) {
        throw std::out_of_range("Index out of range!");
    }
    return operator[](i);
}

template <typename Alloc> bool JsArrayT<Alloc>::operator==(const JsArrayT &rhs) const {
    if (elements.size() != rhs.elements.size()) {
        return false;
    }
    return std::equal(elements.begin(), elements.end(), rhs.elements.begin());
}

template <typename Alloc> bool JsArrayT<Alloc>::Equals(const JsArrayT &rhs, double eps) const {
    if (elements.size() != rhs.elements.size()) {
        return false;
    }
    bool ret = true;
    for (size_t i = 0; i < elements.size() && ret; ++i) {
        ret &= elements[i].Equals(rhs.elements[i], eps);
    }
    return ret;
}

template <typename Alloc> void JsArrayT<Alloc>::Push(JsElementT<Alloc> &&el) { elements.emplace_back(std::move(el)); }

template <typename Alloc> bool JsArrayT<Alloc>::Read(std::istream &in) {
    char c;
    if (!in.read(&c, 1) || c != '[') {
        std::cerr << "JsArray::Read(): Expected '[' instead of " << c << std::endl;
        return false;
    }

    while (in.read(&c, 1)) {
        if (::isspace(c)) {
            continue;
        }
        if (c == ']') {
            return true;
        }
        in.seekg(-1, std::ios::cur);

        elements.emplace_back(JsLiteralType::Null);
        if (!elements.back().Read(in, elements.get_allocator())) {
            return false;
        }

        while (in.read(&c, 1) && isspace(c))
            ;
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

template <typename Alloc> void JsArrayT<Alloc>::Write(std::ostream &out, JsFlags flags) const {
    flags.level++;
    std::string ident_str;
    if (flags.use_new_lines) {
        ident_str += '\n';
    }
    if (flags.ident_levels) {
        for (int i = 0; i < int(flags.level); i++) {
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
        ident_str.resize(ident_str.length() - (flags.use_spaces ? 4 : 1));
    }
    out << ident_str << ']';
}

template struct JsArrayT<std::allocator<char>>;
template struct JsArrayT<Sys::MultiPoolAllocator<char>>;

/////////////////////////////////////////////////////////////////

template <typename Alloc> JsElementT<Alloc> &JsObjectT<Alloc>::operator[](std::string_view s) {
    auto it = std::lower_bound(begin(elements), end(elements), s,
                               [](const ValueType &lhs, std::string_view rhs) { return lhs.first < rhs; });
    if (it == end(elements) || it->first != s) {
        it = elements.emplace(it, StdString<Alloc>{s, elements.get_allocator()}, JsLiteral{JsLiteralType::Null});
    }
    return it->second;
}

template <typename Alloc> const JsElementT<Alloc> &JsObjectT<Alloc>::at(std::string_view s) const {
    auto it = std::lower_bound(begin(elements), end(elements), s,
                               [](const ValueType &lhs, std::string_view rhs) { return lhs.first < rhs; });
    if (it != end(elements) && it->first == s) {
        return it->second;
    }
    throw std::out_of_range(std::string("No such element! \"").append(s) + "\"");
}

template <typename Alloc> JsElementT<Alloc> &JsObjectT<Alloc>::at(std::string_view s) {
    auto it = std::lower_bound(begin(elements), end(elements), s,
                               [](const ValueType &lhs, std::string_view rhs) { return lhs.first < rhs; });
    if (it != end(elements) && it->first == s) {
        return it->second;
    }
    throw std::out_of_range(std::string("No such element! \"").append(s) + "\"");
}

template <typename Alloc> bool JsObjectT<Alloc>::operator==(const JsObjectT &rhs) const {
    if (elements.size() != rhs.elements.size()) {
        return false;
    }
    return std::equal(elements.begin(), elements.end(), rhs.elements.begin());
}

template <typename Alloc> bool JsObjectT<Alloc>::Equals(const JsObjectT &rhs, const double eps) const {
    if (elements.size() != rhs.elements.size()) {
        return false;
    }
    bool ret = true;
    for (size_t i = 0; i < elements.size() && ret; ++i) {
        ret &= elements[i].first == rhs.elements[i].first;
        ret &= elements[i].second.Equals(rhs.elements[i].second, eps);
    }
    return ret;
}

template <typename Alloc> size_t JsObjectT<Alloc>::IndexOf(std::string_view s) const {
    auto it = std::lower_bound(begin(elements), end(elements), s,
                               [](const ValueType &lhs, std::string_view rhs) { return lhs.first < rhs; });
    if (it != end(elements) && it->first == s) {
        return std::distance(begin(elements), it);
    }
    return elements.size();
}

template <typename Alloc> size_t JsObjectT<Alloc>::Insert(std::string_view s, const JsElementT<Alloc> &el) {
    auto it = std::lower_bound(begin(elements), end(elements), s,
                               [](const ValueType &lhs, std::string_view rhs) { return lhs.first < rhs; });
    assert(it == end(elements) || it->first != s);
    it = elements.emplace(it, StdString<Alloc>{s, elements.get_allocator()}, el);
    return std::distance(begin(elements), it);
}

template <typename Alloc> size_t JsObjectT<Alloc>::Insert(std::string_view s, JsElementT<Alloc> &&el) {
    auto it = std::lower_bound(begin(elements), end(elements), s,
                               [](const ValueType &lhs, std::string_view rhs) { return lhs.first < rhs; });
    assert(it == end(elements) || it->first != s);
    it = elements.emplace(it, StdString<Alloc>{s, elements.get_allocator()}, std::move(el));
    return std::distance(begin(elements), it);
}

template <typename Alloc> bool JsObjectT<Alloc>::Read(std::istream &in) {
    char c;
    if (!in.read(&c, 1) || c != '{') {
        std::cerr << "JsObject::Read(): Expected '{' instead of " << c << std::endl;
        return false;
    }
    while (in.read(&c, 1)) {
        if (isspace(c)) {
            continue;
        }
        if (c == '}') {
            return true;
        }
        in.seekg(-1, std::ios::cur);

        JsStringT<Alloc> key(elements.get_allocator());
        if (!key.Read(in)) {
            return false;
        }

        while (in.read(&c, 1) && isspace(c))
            ;
        if (c != ':')
            return false;

        while (in.read(&c, 1) && isspace(c))
            ;
        in.seekg(-1, std::ios::cur);

        const size_t i = Insert(key.val, JsLiteral{JsLiteralType::Null});
        if (!elements[i].second.Read(in, elements.get_allocator())) {
            return false;
        }

        while (in.read(&c, 1) && isspace(c))
            ;
        if (c != ',') {
            return (c == '}');
        }
    }
    return false;
}

template <typename Alloc> void JsObjectT<Alloc>::Write(std::ostream &out, JsFlags flags) const {
    flags.level++;
    std::string ident_str;
    if (flags.use_new_lines) {
        ident_str += '\n';
    }
    if (flags.ident_levels) {
        for (int i = 0; i < int(flags.level); i++) {
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
        ident_str.resize(ident_str.length() - (flags.use_spaces ? 4 : 1));
    }
    out << ident_str << '}';
}

template struct JsObjectT<std::allocator<char>>;
template struct JsObjectT<Sys::MultiPoolAllocator<char>>;

/////////////////////////////////////////////////////////////////

template <typename Alloc> JsElementT<Alloc>::JsElementT(const JsLiteralType lit_type) : type_(JsType::Literal) {
    new (&data_) JsLiteral{lit_type};
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(const double val) : type_(JsType::Number) {
    new (&data_) JsNumber{val};
}

template <typename Alloc>
JsElementT<Alloc>::JsElementT(std::string_view str, const Alloc &alloc) : type_(JsType::String) {
    new (&data_) JsStringT<Alloc>{str, alloc};
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(const JsType type, const Alloc &alloc) : type_(type) {
    if (type_ == JsType::Literal) {
        new (&data_) JsLiteral{JsLiteralType::Null};
    } else if (type_ == JsType::Number) {
        new (&data_) JsNumber{};
    } else if (type_ == JsType::String) {
        new (&data_) JsStringT<Alloc>{alloc};
    } else if (type_ == JsType::Array) {
        new (&data_) JsArrayT<Alloc>{alloc};
    } else if (type_ == JsType::Object) {
        new (&data_) JsObjectT<Alloc>{alloc};
    }
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(const JsLiteral &rhs) {
    new (&data_) JsLiteral{rhs};
    type_ = JsType::Literal;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(const JsNumber &rhs) {
    new (&data_) JsNumber{rhs};
    type_ = JsType::Number;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(const JsStringT<Alloc> &rhs) {
    new (&data_) JsStringT<Alloc>{rhs};
    type_ = JsType::String;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(const JsArrayT<Alloc> &rhs) {
    new (&data_) JsArrayT<Alloc>{rhs};
    type_ = JsType::Array;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(const JsObjectT<Alloc> &rhs) {
    new (&data_) JsObjectT<Alloc>{rhs};
    type_ = JsType::Object;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(const JsElementT<Alloc> &rhs) {
    if (rhs.type_ == JsType::Literal) {
        new (&data_) JsLiteral{reinterpret_cast<const JsLiteral &>(rhs.data_)};
    } else if (rhs.type_ == JsType::Number) {
        new (&data_) JsNumber{reinterpret_cast<const JsNumber &>(rhs.data_)};
    } else if (rhs.type_ == JsType::String) {
        new (&data_) JsStringT<Alloc>{reinterpret_cast<const JsStringT<Alloc> &>(rhs.data_)};
    } else if (rhs.type_ == JsType::Array) {
        new (&data_) JsArrayT<Alloc>{reinterpret_cast<const JsArrayT<Alloc> &>(rhs.data_)};
    } else if (rhs.type_ == JsType::Object) {
        new (&data_) JsObjectT<Alloc>{reinterpret_cast<const JsObjectT<Alloc> &>(rhs.data_)};
    }
    type_ = rhs.type_;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(JsStringT<Alloc> &&rhs) {
    new (&data_) JsStringT<Alloc>{std::move(rhs)};
    type_ = JsType::String;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(JsArrayT<Alloc> &&rhs) {
    new (&data_) JsArrayT<Alloc>{std::move(rhs)};
    type_ = JsType::Array;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(JsObjectT<Alloc> &&rhs) {
    new (&data_) JsObjectT<Alloc>{std::move(rhs)};
    type_ = JsType::Object;
}

template <typename Alloc> JsElementT<Alloc>::JsElementT(JsElementT<Alloc> &&rhs) noexcept {
    if (rhs.type_ == JsType::Literal) {
        new (&data_) JsLiteral{reinterpret_cast<const JsLiteral &>(rhs.data_)};
    } else if (rhs.type_ == JsType::Number) {
        new (&data_) JsNumber{reinterpret_cast<const JsNumber &>(rhs.data_)};
    } else if (rhs.type_ == JsType::String) {
        new (&data_) JsStringT<Alloc>{std::move(reinterpret_cast<JsStringT<Alloc> &>(rhs.data_))};
    } else if (rhs.type_ == JsType::Array) {
        new (&data_) JsArrayT<Alloc>{std::move(reinterpret_cast<JsArrayT<Alloc> &>(rhs.data_))};
    } else if (rhs.type_ == JsType::Object) {
        new (&data_) JsObjectT<Alloc>{std::move(reinterpret_cast<JsObjectT<Alloc> &>(rhs.data_))};
    }
    type_ = rhs.type_;
}

template <typename Alloc> JsElementT<Alloc>::~JsElementT() { Destroy(); }

template <typename Alloc> void JsElementT<Alloc>::Destroy() {
    if (type_ == JsType::Literal) {
        reinterpret_cast<JsLiteral &>(data_).~JsLiteral();
    } else if (type_ == JsType::Number) {
        reinterpret_cast<JsNumber &>(data_).~JsNumber();
    } else if (type_ == JsType::String) {
        reinterpret_cast<JsStringT<Alloc> &>(data_).~JsStringT();
    } else if (type_ == JsType::Array) {
        reinterpret_cast<JsArrayT<Alloc> &>(data_).~JsArrayT();
    } else if (type_ == JsType::Object) {
        reinterpret_cast<JsObjectT<Alloc> &>(data_).~JsObjectT();
    }
    type_ = JsType::Invalid;
}

template <typename Alloc> JsLiteral &JsElementT<Alloc>::as_lit() {
    if (type_ != JsType::Literal) {
        throw std::bad_cast();
    }
    return reinterpret_cast<JsLiteral &>(data_);
}

template <typename Alloc> JsNumber &JsElementT<Alloc>::as_num() {
    if (type_ != JsType::Number) {
        throw std::bad_cast();
    }
    return reinterpret_cast<JsNumber &>(data_);
}

template <typename Alloc> JsStringT<Alloc> &JsElementT<Alloc>::as_str() {
    if (type_ != JsType::String) {
        throw std::bad_cast();
    }
    return reinterpret_cast<JsStringT<Alloc> &>(data_);
}

template <typename Alloc> JsArrayT<Alloc> &JsElementT<Alloc>::as_arr() {
    if (type_ != JsType::Array) {
        throw std::bad_cast();
    }
    return reinterpret_cast<JsArrayT<Alloc> &>(data_);
}

template <typename Alloc> JsObjectT<Alloc> &JsElementT<Alloc>::as_obj() {
    if (type_ != JsType::Object) {
        throw std::bad_cast();
    }
    return reinterpret_cast<JsObjectT<Alloc> &>(data_);
}

//

template <typename Alloc> const JsLiteral &JsElementT<Alloc>::as_lit() const {
    if (type_ != JsType::Literal) {
        throw std::bad_cast();
    }
    return reinterpret_cast<const JsLiteral &>(data_);
}

template <typename Alloc> const JsNumber &JsElementT<Alloc>::as_num() const {
    if (type_ != JsType::Number) {
        throw std::bad_cast();
    }
    return reinterpret_cast<const JsNumber &>(data_);
}

template <typename Alloc> const JsStringT<Alloc> &JsElementT<Alloc>::as_str() const {
    if (type_ != JsType::String) {
        throw std::bad_cast();
    }
    return reinterpret_cast<const JsStringT<Alloc> &>(data_);
}

template <typename Alloc> const JsArrayT<Alloc> &JsElementT<Alloc>::as_arr() const {
    if (type_ != JsType::Array) {
        throw std::bad_cast();
    }
    return reinterpret_cast<const JsArrayT<Alloc> &>(data_);
}

template <typename Alloc> const JsObjectT<Alloc> &JsElementT<Alloc>::as_obj() const {
    if (type_ != JsType::Object) {
        throw std::bad_cast();
    }
    return reinterpret_cast<const JsObjectT<Alloc> &>(data_);
}

//

template <typename Alloc> JsElementT<Alloc> &JsElementT<Alloc>::operator=(JsElementT &&rhs) noexcept {
    Destroy();
    if (rhs.type_ == JsType::Literal) {
        new (&data_) JsLiteral{reinterpret_cast<JsLiteral &>(rhs.data_)};
    } else if (rhs.type_ == JsType::Number) {
        new (&data_) JsNumber{reinterpret_cast<JsNumber &>(rhs.data_)};
    } else if (rhs.type_ == JsType::String) {
        new (&data_) JsStringT<Alloc>{std::move(reinterpret_cast<JsStringT<Alloc> &>(rhs.data_))};
    } else if (rhs.type_ == JsType::Array) {
        new (&data_) JsArrayT<Alloc>{std::move(reinterpret_cast<JsArrayT<Alloc> &>(rhs.data_))};
    } else if (rhs.type_ == JsType::Object) {
        new (&data_) JsObjectT<Alloc>{std::move(reinterpret_cast<JsObjectT<Alloc> &>(rhs.data_))};
    }
    type_ = rhs.type_;
    return *this;
}

template <typename Alloc> JsElementT<Alloc> &JsElementT<Alloc>::operator=(const JsElementT &rhs) {
    if (&rhs == this) {
        return *this;
    }

    Destroy();
    if (rhs.type_ == JsType::Literal) {
        new (&data_) JsLiteral{reinterpret_cast<const JsLiteral &>(rhs.data_)};
    } else if (rhs.type_ == JsType::Number) {
        new (&data_) JsNumber{reinterpret_cast<const JsNumber &>(rhs.data_)};
    } else if (rhs.type_ == JsType::String) {
        new (&data_) JsStringT<Alloc>{reinterpret_cast<const JsStringT<Alloc> &>(rhs.data_)};
    } else if (rhs.type_ == JsType::Array) {
        new (&data_) JsArrayT<Alloc>{reinterpret_cast<const JsArrayT<Alloc> &>(rhs.data_)};
    } else if (rhs.type_ == JsType::Object) {
        new (&data_) JsObjectT<Alloc>{reinterpret_cast<const JsObjectT<Alloc> &>(rhs.data_)};
    }
    type_ = rhs.type_;
    return *this;
}

template <typename Alloc> bool JsElementT<Alloc>::operator==(const JsElementT &rhs) const {
    if (type_ != rhs.type_)
        return false;

    if (type_ == JsType::Literal) {
        return reinterpret_cast<const JsLiteral &>(data_) == reinterpret_cast<const JsLiteral &>(rhs.data_);
    } else if (type_ == JsType::Number) {
        return reinterpret_cast<const JsNumber &>(data_) == reinterpret_cast<const JsNumber &>(rhs.data_);
    } else if (type_ == JsType::String) {
        return reinterpret_cast<const JsStringT<Alloc> &>(data_) ==
               reinterpret_cast<const JsStringT<Alloc> &>(rhs.data_);
    } else if (type_ == JsType::Array) {
        return reinterpret_cast<const JsArrayT<Alloc> &>(data_) == reinterpret_cast<const JsArrayT<Alloc> &>(rhs.data_);
    } else if (type_ == JsType::Object) {
        return reinterpret_cast<const JsObjectT<Alloc> &>(data_) ==
               reinterpret_cast<const JsObjectT<Alloc> &>(rhs.data_);
    }

    return false;
}

template <typename Alloc> bool JsElementT<Alloc>::Equals(const JsElementT &rhs, const double eps) const {
    if (type_ != rhs.type_)
        return false;

    if (type_ == JsType::Literal) {
        return reinterpret_cast<const JsLiteral &>(data_) == reinterpret_cast<const JsLiteral &>(rhs.data_);
    } else if (type_ == JsType::Number) {
        return reinterpret_cast<const JsNumber &>(data_).Equals(reinterpret_cast<const JsNumber &>(rhs.data_), eps);
    } else if (type_ == JsType::String) {
        return reinterpret_cast<const JsStringT<Alloc> &>(data_) ==
               reinterpret_cast<const JsStringT<Alloc> &>(rhs.data_);
    } else if (type_ == JsType::Array) {
        return reinterpret_cast<const JsArrayT<Alloc> &>(data_).Equals(
            reinterpret_cast<const JsArrayT<Alloc> &>(rhs.data_), eps);
    } else if (type_ == JsType::Object) {
        return reinterpret_cast<const JsObjectT<Alloc> &>(data_).Equals(
            reinterpret_cast<const JsObjectT<Alloc> &>(rhs.data_), eps);
    }

    return false;
}

template <typename Alloc> bool JsElementT<Alloc>::Read(std::istream &in, const Alloc &alloc) {
    Destroy();

    char c;
    while (in.read(&c, 1) && isspace(c))
        ;
    in.seekg(-1, std::ios::cur);
    if (c == '\"') {
        new (&data_) JsStringT<Alloc>{alloc};
        type_ = JsType::String;
        return reinterpret_cast<JsStringT<Alloc> &>(data_).Read(in);
    } else if (c == '[') {
        new (&data_) JsArrayT<Alloc>{alloc};
        type_ = JsType::Array;
        return reinterpret_cast<JsArrayT<Alloc> &>(data_).Read(in);
    } else if (c == '{') {
        new (&data_) JsObjectT<Alloc>{alloc};
        type_ = JsType::Object;
        return reinterpret_cast<JsObjectT<Alloc> &>(data_).Read(in);
    } else {
        if (isdigit(c) || c == '-') {
            new (&data_) JsNumber{};
            type_ = JsType::Number;
            return reinterpret_cast<JsNumber &>(data_).Read(in);
        } else {
            new (&data_) JsLiteral{JsLiteralType::Null};
            type_ = JsType::Literal;
            return reinterpret_cast<JsLiteral &>(data_).Read(in);
        }
    }
}

template <typename Alloc> void JsElementT<Alloc>::Write(std::ostream &out, const JsFlags flags) const {
    if (type_ == JsType::Literal) {
        reinterpret_cast<const JsLiteral &>(data_).Write(out, flags);
    } else if (type_ == JsType::Number) {
        reinterpret_cast<const JsNumber &>(data_).Write(out, flags);
    } else if (type_ == JsType::String) {
        reinterpret_cast<const JsStringT<Alloc> &>(data_).Write(out, flags);
    } else if (type_ == JsType::Array) {
        reinterpret_cast<const JsArrayT<Alloc> &>(data_).Write(out, flags);
    } else if (type_ == JsType::Object) {
        reinterpret_cast<const JsObjectT<Alloc> &>(data_).Write(out, flags);
    }
}

template struct JsElementT<std::allocator<char>>;
template struct JsElementT<Sys::MultiPoolAllocator<char>>;