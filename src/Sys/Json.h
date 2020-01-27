#pragma once

#include <iosfwd>
#include <list>
#include <string>
#include <utility>

#include "Variant.h"

enum JsType { JS_INVALID, JS_OBJECT, JS_ARRAY, JS_NUMBER, JS_LITERAL, JS_STRING };
enum JsLiteralType { JS_TRUE, JS_FALSE, JS_NULL };

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

    explicit JsLiteral(JsLiteralType v) : val(v) {}
    bool operator==(const JsLiteral &rhs) const {
        return val == rhs.val;
    }

    bool operator!=(const JsLiteral &rhs) const {
        return val != rhs.val;
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JS_LITERAL;
};

struct JsNumber {
    double val;

    explicit JsNumber(double v = 0) : val(v) {}

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

    static const JsType type = JS_NUMBER;
};

struct JsString {
    std::string val;

    JsString() {}
    explicit JsString(const char *s) : val(s) {}
    explicit JsString(const std::string &s) : val(s) {}

    bool operator==(const JsString &rhs) const {
        return val == rhs.val;
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JS_STRING;
};

struct JsArray {
    std::list<JsElement> elements;

    JsArray() {}
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

    void Push(JsElement &&el) {
        elements.emplace_back(std::move(el));
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JS_ARRAY;
};

struct JsObject {
    std::list<std::pair<std::string, JsElement>> elements;

    std::pair<std::string, JsElement> &operator[](size_t i);
    JsElement &operator[](const std::string &s);

    const JsElement &at(const std::string &s) const;
    JsElement &at(const std::string &s);

    bool operator==(const JsObject &rhs) const;
    bool operator!=(const JsObject &rhs) const {
        return !operator==(rhs);
    }

    bool Has(const std::string &s) const;

    size_t Size() const {
        return elements.size();
    }

    void Push(const std::string &s, const JsElement &el);
    void Push(const std::string &s, JsElement &&el);

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;

    static const JsType type = JS_OBJECT;
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
    JsElement(const JsNumber &rhs);
    JsElement(const JsString &rhs);
    JsElement(const JsArray &rhs);
    JsElement(const JsObject &rhs);
    JsElement(const JsLiteral &rhs);
    JsElement(const JsElement &rhs);

    JsElement(JsString &&rhs);
    JsElement(JsArray &&rhs);
    JsElement(JsObject &&rhs);
    JsElement(JsElement &&rhs);

    ~JsElement();

    JsType type() const {
        return type_;
    }

    explicit operator JsLiteral &();
    explicit operator JsNumber &();
    explicit operator JsString &();
    explicit operator JsArray &();
    explicit operator JsObject &();
    
    explicit operator const JsLiteral &() const;
    explicit operator const JsNumber &() const;
    explicit operator const JsString &() const;
    explicit operator const JsArray &() const;
    explicit operator const JsObject &() const;

    JsElement &operator=(JsElement &&rhs) noexcept;
    JsElement &operator=(const JsElement &rhs);

    bool operator==(const JsElement &rhs) const;

    bool operator!=(const JsElement &rhs) const {
        return !operator==(rhs);
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;
};

