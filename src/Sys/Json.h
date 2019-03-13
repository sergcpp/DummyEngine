#pragma once

#include <iosfwd>
#include <list>
#include <string>
#include <utility>

enum JsType { JS_OBJECT, JS_ARRAY, JS_NUMBER, JS_LITERAL, JS_STRING };
enum JsLiteralType { JS_TRUE, JS_FALSE, JS_NULL };

struct JsFlags {
    int ident_levels : 1;
    int use_new_lines : 1;
    int level : 30;

    JsFlags() : ident_levels(1), use_new_lines(1), level(0) {}
};

struct JsElement;

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
};

struct JsArray {
    std::list<JsElement> elements;

    JsArray() {}
    JsArray(const JsElement *v, size_t num);
    JsArray(const std::initializer_list<JsElement> &l);
    JsArray(std::initializer_list<JsElement> &&l);

    JsElement &operator[](size_t i) {
        auto it = elements.begin();
        std::advance(it, i);
        return *it;
    }

    const JsElement &operator[](size_t i) const {
        auto it = elements.cbegin();
        std::advance(it, i);
        return *it;
    }

    const JsElement &at(size_t i) const;

    bool operator==(const JsArray &rhs) const;
    bool operator!=(const JsArray &rhs) const {
        return !operator==(rhs);
    }

    size_t Size() const {
        return elements.size();
    }

    void Push(const JsElement &e) {
        elements.push_back(e);
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;
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

    void Push(const std::string &s, const JsElement &e);

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;
};

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
};

struct JsElement {
private:
    JsType type_;
    union {
        void        *p_;
        JsNumber    *num_;
        JsString    *str_;
        JsArray     *arr_;
        JsObject    *obj_;
        JsLiteral   *lit_;
    };

    void DestroyValue();
public:
    explicit JsElement(double val);
    explicit JsElement(const char *str);
    explicit JsElement(JsLiteralType lit_type);
    explicit JsElement(JsType type);
    JsElement(const JsNumber &rhs);
    JsElement(const JsString &rhs);
    JsElement(const JsArray &rhs);
    JsElement(const JsObject &rhs);
    JsElement(const JsLiteral &rhs);
    JsElement(const JsElement &rhs);

    ~JsElement();

    JsType type() const {
        return type_;
    }

    operator JsNumber &();
    operator JsString &();
    operator JsArray &();
    operator JsObject &();
    operator JsLiteral &();

    operator const JsNumber &() const;
    operator const JsString &() const;
    operator const JsArray &() const;
    operator const JsObject &() const;
    operator const JsLiteral &() const;

    JsElement &operator=(JsElement &&rhs);
    JsElement &operator=(const JsElement &rhs);

    bool operator==(const JsElement &rhs) const;

    bool operator!=(const JsElement &rhs) const {
        return !operator==(rhs);
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = JsFlags()) const;
};

