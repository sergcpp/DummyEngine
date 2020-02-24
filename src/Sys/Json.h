#pragma once

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "Variant.h"

enum class JsType { Invalid, Object, Array, Number, Literal, String };
enum class JsLiteralType { Undefined, True, False, Null };

struct JsFlags {
    int ident_levels    : 1;
    int use_new_lines   : 1;
    int use_spaces      : 1;
    int level           : 29;

    JsFlags() : ident_levels(1), use_new_lines(1), use_spaces(0), level(0) {}
};
static_assert(sizeof(JsFlags) == 4, "!");

struct JsElement;

struct JsLiteral {
    JsLiteralType val;

    explicit JsLiteral(const JsLiteralType v) : val(v) {}
    bool operator==(const JsLiteral &rhs) const {
        return val == rhs.val;
    }

    bool operator!=(const JsLiteral &rhs) const {
        return val != rhs.val;
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JsType::Literal;
};

struct JsNumber {
    double val;

    explicit JsNumber(const double v = 0.0) : val(v) {}

    bool operator==(const JsNumber &rhs) const {
        return val == rhs.val;
    }

    bool operator==(const double &rhs) const {
        return val == rhs;
    }

    bool operator==(const int rhs) const {
        return val == rhs;
    }

    operator double &() {
        return val;
    }

    operator double() const {
        return val;
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JsType::Number;
};

struct JsString {
    std::string val;

    JsString() = default;
    explicit JsString(const char *s) : val(s) {}
    explicit JsString(std::string s) : val(std::move(s)) {}

    bool operator==(const JsString &rhs) const {
        return val == rhs.val;
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JsType::String;
};

struct JsArray {
    std::vector<JsElement> elements;

    JsArray() = default;
    JsArray(const JsArray &rhs) = default;
    JsArray(JsArray &&rhs) = default;
    JsArray(const JsElement *v, size_t num);
    //JsArray(const std::initializer_list<JsElement> &l);
    //JsArray(std::initializer_list<JsElement> &&l);

    JsElement &operator[](size_t i);
    const JsElement &operator[](size_t i) const;

    const JsElement &at(size_t i) const;

    bool operator==(const JsArray &rhs) const;
    bool operator!=(const JsArray &rhs) const {
        return !operator==(rhs);
    }

    size_t Size() const {
        return elements.size();
    }

    void Push(const JsElement &el) {
        elements.push_back(el);
    }

    void Push(JsElement&& el);

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JsType::Array;
};

struct JsObject {
    std::vector<std::pair<std::string, JsElement>> elements;

    std::pair<std::string, JsElement> &operator[](size_t i);
    JsElement &operator[](const std::string &s);

    const JsElement &at(const char *s) const;
    JsElement &at(const char *s);
    const JsElement &at(const std::string &s) const;
    JsElement &at(const std::string &s);

    bool operator==(const JsObject &rhs) const;
    bool operator!=(const JsObject &rhs) const {
        return !operator==(rhs);
    }

    bool Has(const char *s) const;
    bool Has(const std::string &s) const;

    size_t Size() const {
        return elements.size();
    }

    size_t Push(const std::string &s, const JsElement &el);
    size_t Push(const std::string &s, JsElement &&el);

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JsType::Object;
};

struct JsElement {
private:
    static const size_t
        data_size = Sys::_compile_time_max<sizeof(JsLiteral), sizeof(JsNumber), sizeof(JsString), sizeof(JsArray), sizeof(JsObject)>::value,
        data_align = Sys::_compile_time_max<alignof(JsLiteral), alignof(JsNumber), alignof(JsString), alignof(JsArray), alignof(JsObject)>::value;
    using data_t = typename std::aligned_storage<data_size, data_align>::type;

    JsType type_;
    data_t data_;

    void Destroy();
public:
    explicit JsElement(JsLiteralType lit_type);
    explicit JsElement(double val);
    explicit JsElement(const char *str);
    explicit JsElement(JsType type);
    JsElement(const JsNumber &rhs); // NOLINT
    JsElement(const JsString &rhs); // NOLINT
    JsElement(const JsArray &rhs); // NOLINT
    JsElement(const JsObject &rhs); // NOLINT
    JsElement(const JsLiteral &rhs); // NOLINT
    JsElement(const JsElement &rhs);

    JsElement(JsString &&rhs);
    JsElement(JsArray &&rhs);
    JsElement(JsObject &&rhs);
    JsElement(JsElement &&rhs);

    ~JsElement();

    JsType type() const {
        return type_;
    }

    JsLiteral &as_lit();
    JsNumber &as_num();
    JsString &as_str();
    JsArray &as_arr();
    JsObject &as_obj();

    const JsLiteral &as_lit() const;
    const JsNumber &as_num() const;
    const JsString &as_str() const;
    const JsArray &as_arr() const;
    const JsObject &as_obj() const;

    JsElement &operator=(JsElement &&rhs) noexcept;
    JsElement &operator=(const JsElement &rhs);

    bool operator==(const JsElement &rhs) const;

    bool operator!=(const JsElement &rhs) const {
        return !operator==(rhs);
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;
};

