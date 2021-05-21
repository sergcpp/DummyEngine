#pragma once

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "PoolAlloc.h"
#include "Variant.h"

enum class JsType { Invalid = -1, Literal, Number, String, Array, Object };
enum class JsLiteralType { Undefined, True, False, Null };

struct JsFlags {
    int ident_levels : 1;
    int use_new_lines : 1;
    int use_spaces : 1;
    int level : 29;

    JsFlags() : ident_levels(1), use_new_lines(1), use_spaces(0), level(0) {}
};
static_assert(sizeof(JsFlags) == 4, "!");

template <typename Alloc> struct JsElementT;

struct JsLiteral {
    JsLiteralType val;

    explicit JsLiteral(const JsLiteralType v) : val(v) {}

    bool operator==(const JsLiteral &rhs) const { return val == rhs.val; }
    bool operator!=(const JsLiteral &rhs) const { return val != rhs.val; }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    static const JsType type = JsType::Literal;
};

struct JsNumber {
    double val;

    explicit JsNumber(const double v = 0.0) : val(v) {}
    explicit JsNumber(const float v) : val(double(v)) {}
    explicit JsNumber(const int v) : val(double(v)) {}

    bool operator==(const JsNumber &rhs) const { return val == rhs.val; }
    bool operator==(const double &rhs) const { return val == rhs; }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    static const JsType type = JsType::Number;
};

template <typename Alloc>
using StdString = std::basic_string<char, std::char_traits<char>, Alloc>;
using StdStringP = StdString<Sys::MultiPoolAllocator<char>>;

inline bool operator==(const std::string &lhs, const StdStringP &rhs) {
    return lhs.compare(rhs.c_str()) == 0;
}

inline bool operator==(const StdStringP &lhs, const std::string &rhs) {
    return lhs.compare(rhs.c_str()) == 0;
}

template <typename Alloc> struct JsStringT {
    StdString<Alloc> val;

    JsStringT(const Alloc &alloc = Alloc()) : val(alloc) {}
    JsStringT(const JsStringT &rhs) = default;
    JsStringT(JsStringT &&rhs) = default;

    JsStringT &operator=(const JsStringT &rhs) = default;
    JsStringT &operator=(JsStringT &&rhs) = default;

    explicit JsStringT(const char *s, const Alloc &alloc = Alloc()) : val(s, alloc) {}
    explicit JsStringT(StdString<Alloc> s, const Alloc &alloc = Alloc())
        : val(std::move(s), alloc) {}

    bool operator==(const JsStringT &rhs) const { return val == rhs.val; }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    static const JsType type = JsType::String;
};
extern template struct JsStringT<std::allocator<char>>;
extern template struct JsStringT<Sys::MultiPoolAllocator<char>>;

template <typename Alloc> struct JsArrayT {
    std::vector<JsElementT<Alloc>, Alloc> elements;

    JsArrayT(const Alloc &alloc = Alloc()) : elements(alloc) {}
    JsArrayT(const JsArrayT &rhs) = default;
    JsArrayT(JsArrayT &&rhs) = default;
    JsArrayT(const JsElementT<Alloc> *v, size_t num, const Alloc &alloc = Alloc());

    JsElementT<Alloc> &operator[](size_t i);
    const JsElementT<Alloc> &operator[](size_t i) const;

    const JsElementT<Alloc> &at(size_t i) const;

    bool operator==(const JsArrayT &rhs) const;
    bool operator!=(const JsArrayT &rhs) const { return !operator==(rhs); }

    size_t Size() const { return elements.size(); }

    void Push(const JsElementT<Alloc> &el) { elements.push_back(el); }

    void Push(JsElementT<Alloc> &&el);

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    static const JsType type = JsType::Array;
};
extern template struct JsArrayT<std::allocator<char>>;
extern template struct JsArrayT<Sys::MultiPoolAllocator<char>>;

template <typename Alloc> struct JsObjectT {
    std::vector<std::pair<StdString<Alloc>, JsElementT<Alloc>>, Alloc> elements;

    JsObjectT(const Alloc &alloc = Alloc()) : elements(alloc) {}

    const std::pair<StdString<Alloc>, JsElementT<Alloc>> &operator[](size_t i) const;
    std::pair<StdString<Alloc>, JsElementT<Alloc>> &operator[](size_t i);
    JsElementT<Alloc> &operator[](const StdString<Alloc> &s);
    JsElementT<Alloc> &operator[](const char *s);

    const JsElementT<Alloc> &at(const char *s) const;
    JsElementT<Alloc> &at(const char *s);
    const JsElementT<Alloc> &at(const StdString<Alloc> &s) const;
    JsElementT<Alloc> &at(const StdString<Alloc> &s);

    bool operator==(const JsObjectT<Alloc> &rhs) const;
    bool operator!=(const JsObjectT<Alloc> &rhs) const { return !operator==(rhs); }

    bool Has(const char *s) const;
    bool Has(const StdString<Alloc> &s) const;

    size_t Size() const { return elements.size(); }

    size_t Push(const StdString<Alloc> &s, const JsElementT<Alloc> &el);
    size_t Push(const char *s, const JsElementT<Alloc> &el) {
        return Push(StdString<Alloc>(s, elements.get_allocator()), el);
    }
    size_t Push(const StdString<Alloc> &s, JsElementT<Alloc> &&el);
    size_t Push(const char *s, JsElementT<Alloc> &&el) {
        return Push(StdString<Alloc>(s, elements.get_allocator()), std::move(el));
    }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    static const JsType type = JsType::Object;
};
extern template struct JsObjectT<std::allocator<char>>;
extern template struct JsObjectT<Sys::MultiPoolAllocator<char>>;

template <typename Alloc> struct JsElementT {
  private:
    static const size_t
        data_size =
            Sys::_compile_time_max<sizeof(JsLiteral), sizeof(JsNumber),
                                   sizeof(JsStringT<Alloc>), sizeof(JsArrayT<Alloc>),
                                   sizeof(JsObjectT<Alloc>)>::value,
        data_align =
            Sys::_compile_time_max<alignof(JsLiteral), alignof(JsNumber),
                                   alignof(JsStringT<Alloc>), alignof(JsArrayT<Alloc>),
                                   alignof(JsObjectT<Alloc>)>::value;
    using data_t = typename std::aligned_storage<data_size, data_align>::type;

    JsType type_;
    data_t data_;

    void Destroy();

  public:
    explicit JsElementT(JsLiteralType lit_type);
    explicit JsElementT(double val);
    explicit JsElementT(const char *str, const Alloc &alloc = Alloc());
    explicit JsElementT(JsType type, const Alloc &alloc = Alloc());
    JsElementT(const JsNumber &rhs);         // NOLINT
    JsElementT(const JsStringT<Alloc> &rhs); // NOLINT
    JsElementT(const JsArrayT<Alloc> &rhs);  // NOLINT
    JsElementT(const JsObjectT<Alloc> &rhs); // NOLINT
    JsElementT(const JsLiteral &rhs);        // NOLINT
    JsElementT(const JsElementT &rhs);

    JsElementT(JsStringT<Alloc> &&rhs);
    JsElementT(JsArrayT<Alloc> &&rhs);
    JsElementT(JsObjectT<Alloc> &&rhs);
    JsElementT(JsElementT &&rhs);

    ~JsElementT();

    JsType type() const { return type_; }

    JsLiteral &as_lit();
    JsNumber &as_num();
    JsStringT<Alloc> &as_str();
    JsArrayT<Alloc> &as_arr();
    JsObjectT<Alloc> &as_obj();

    const JsLiteral &as_lit() const;
    const JsNumber &as_num() const;
    const JsStringT<Alloc> &as_str() const;
    const JsArrayT<Alloc> &as_arr() const;
    const JsObjectT<Alloc> &as_obj() const;

    JsElementT &operator=(JsElementT &&rhs) noexcept;
    JsElementT &operator=(const JsElementT &rhs);

    bool operator==(const JsElementT &rhs) const;
    bool operator!=(const JsElementT &rhs) const { return !operator==(rhs); }

    bool Read(std::istream &in, const Alloc &alloc = Alloc());
    void Write(std::ostream &out, JsFlags flags = {}) const;
};

extern template struct JsElementT<std::allocator<char>>;
extern template struct JsElementT<Sys::MultiPoolAllocator<char>>;

using JsString = JsStringT<std::allocator<char>>;
using JsStringP = JsStringT<Sys::MultiPoolAllocator<char>>;

using JsArray = JsArrayT<std::allocator<char>>;
using JsArrayP = JsArrayT<Sys::MultiPoolAllocator<char>>;

using JsObject = JsObjectT<std::allocator<char>>;
using JsObjectP = JsObjectT<Sys::MultiPoolAllocator<char>>;

using JsElement = JsElementT<std::allocator<char>>;
using JsElementP = JsElementT<Sys::MultiPoolAllocator<char>>;