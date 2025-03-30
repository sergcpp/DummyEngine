#pragma once

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "PoolAlloc.h"

namespace Sys {
template <size_t arg1, size_t... args> struct _compile_time_max;
template <size_t arg> struct _compile_time_max<arg> {
    static const size_t value = arg;
};
template <size_t arg1, size_t arg2, size_t... args> struct _compile_time_max<arg1, arg2, args...> {
    static const size_t value =
        arg1 >= arg2 ? _compile_time_max<arg1, args...>::value : _compile_time_max<arg2, args...>::value;
};

enum class JsType { Invalid = -1, Literal, Number, String, Array, Object };
enum class JsLiteralType { Undefined, True, False, Null };

struct JsFlags {
    unsigned ident_levels : 1;
    unsigned use_new_lines : 1;
    unsigned use_identation : 1;
    unsigned prefer_spaces : 1;
    unsigned level : 28;

    JsFlags() : ident_levels(1), use_new_lines(1), use_identation(1), prefer_spaces(0), level(0) {}
};
static_assert(sizeof(JsFlags) == 4);

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

    [[nodiscard]] bool Equals(const JsNumber &rhs, const double eps) const { return std::abs(val - rhs.val) < eps; }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    static const JsType type = JsType::Number;
};

template <typename Alloc> using StdString = std::basic_string<char, std::char_traits<char>, Alloc>;
using StdStringP = StdString<Sys::MultiPoolAllocator<char>>;

inline bool operator==(const std::string &lhs, const StdStringP &rhs) { return lhs.compare(rhs.c_str()) == 0; }
inline bool operator!=(const std::string &lhs, const StdStringP &rhs) { return !operator==(lhs, rhs); }

inline bool operator==(const StdStringP &lhs, const std::string &rhs) { return lhs.compare(rhs.c_str()) == 0; }
inline bool operator!=(const StdStringP &lhs, const std::string &rhs) { return !operator==(lhs, rhs); }

template <typename Alloc> struct JsStringT {
    StdString<Alloc> val;

    explicit JsStringT(const Alloc &alloc = Alloc()) : val(alloc) {}
    JsStringT(const JsStringT &rhs) = default;
    JsStringT(JsStringT &&rhs) noexcept = default;

    JsStringT &operator=(const JsStringT &rhs) = default;
    JsStringT &operator=(JsStringT &&rhs) noexcept = default;

    explicit JsStringT(std::string_view s, const Alloc &alloc = Alloc()) : val(s, alloc) {}

    bool operator==(const JsStringT &rhs) const { return val == rhs.val; }

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    Alloc get_allocator() { return val.get_allocator(); }

    static const JsType type = JsType::String;
};
extern template struct JsStringT<std::allocator<char>>;
extern template struct JsStringT<Sys::MultiPoolAllocator<char>>;

template <typename Alloc> struct JsArrayT {
    using AllocV = typename std::allocator_traits<Alloc>::template rebind_alloc<JsElementT<Alloc>>;
    std::vector<JsElementT<Alloc>, AllocV> elements;

    explicit JsArrayT(const Alloc &alloc = Alloc()) : elements(alloc) {}
    JsArrayT(const JsArrayT &rhs) = default;
    JsArrayT(JsArrayT &&rhs) noexcept = default;
    JsArrayT(const JsElementT<Alloc> *v, size_t num, const Alloc &alloc = Alloc());

    JsElementT<Alloc> &operator[](size_t i);
    const JsElementT<Alloc> &operator[](size_t i) const;

    [[nodiscard]] const JsElementT<Alloc> &at(size_t i) const;

    bool operator==(const JsArrayT &rhs) const;
    bool operator!=(const JsArrayT &rhs) const { return !operator==(rhs); }

    [[nodiscard]] bool Equals(const JsArrayT &rhs, double eps) const;

    [[nodiscard]] size_t Size() const { return elements.size(); }

    void Push(const JsElementT<Alloc> &el) { elements.push_back(el); }
    void Push(JsElementT<Alloc> &&el);

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    Alloc get_allocator() { return elements.get_allocator(); }

    static const JsType type = JsType::Array;
};
extern template struct JsArrayT<std::allocator<char>>;
extern template struct JsArrayT<Sys::MultiPoolAllocator<char>>;

template <typename Alloc> struct JsObjectT {
    using ValueType = std::pair<StdString<Alloc>, JsElementT<Alloc>>;
    using AllocV = typename std::allocator_traits<Alloc>::template rebind_alloc<ValueType>;
    std::vector<ValueType, AllocV> elements;

    explicit JsObjectT(const Alloc &alloc = Alloc()) : elements(alloc) {}

    const std::pair<StdString<Alloc>, JsElementT<Alloc>> &operator[](size_t i) const { return elements[i]; }
    std::pair<StdString<Alloc>, JsElementT<Alloc>> &operator[](size_t i) { return elements[i]; }
    JsElementT<Alloc> &operator[](std::string_view s);

    [[nodiscard]] const JsElementT<Alloc> &at(std::string_view s) const;
    [[nodiscard]] JsElementT<Alloc> &at(std::string_view s);

    bool operator==(const JsObjectT<Alloc> &rhs) const;
    bool operator!=(const JsObjectT<Alloc> &rhs) const { return !operator==(rhs); }

    [[nodiscard]] bool Equals(const JsObjectT &rhs, double eps) const;

    [[nodiscard]] bool Has(std::string_view s) const { return IndexOf(s) < Size(); }

    [[nodiscard]] size_t IndexOf(std::string_view s) const;

    [[nodiscard]] size_t Size() const { return elements.size(); }

    size_t Insert(std::string_view s, const JsElementT<Alloc> &el);
    size_t Insert(std::string_view s, JsElementT<Alloc> &&el);

    bool Read(std::istream &in);
    void Write(std::ostream &out, JsFlags flags = {}) const;

    Alloc get_allocator() { return elements.get_allocator(); }

    static const JsType type = JsType::Object;
};
extern template struct JsObjectT<std::allocator<char>>;
extern template struct JsObjectT<Sys::MultiPoolAllocator<char>>;

template <typename Alloc> struct JsElementT {
  private:
    static const size_t data_size =
                            Sys::_compile_time_max<sizeof(JsLiteral), sizeof(JsNumber), sizeof(JsStringT<Alloc>),
                                                   sizeof(JsArrayT<Alloc>), sizeof(JsObjectT<Alloc>)>::value,
                        data_align =
                            Sys::_compile_time_max<alignof(JsLiteral), alignof(JsNumber), alignof(JsStringT<Alloc>),
                                                   alignof(JsArrayT<Alloc>), alignof(JsObjectT<Alloc>)>::value;
    using data_t = typename std::aligned_storage<data_size, data_align>::type;

    JsType type_;
    data_t data_;

    void Destroy();

  public:
    explicit JsElementT(JsLiteralType lit_type);
    explicit JsElementT(double val);
    explicit JsElementT(std::string_view str, const Alloc &alloc = Alloc());
    explicit JsElementT(JsType type, const Alloc &alloc = Alloc());
    JsElementT(const JsNumber &rhs);         // NOLINT
    JsElementT(const JsStringT<Alloc> &rhs); // NOLINT
    JsElementT(const JsArrayT<Alloc> &rhs);  // NOLINT
    JsElementT(const JsObjectT<Alloc> &rhs); // NOLINT
    JsElementT(const JsLiteral &rhs);        // NOLINT
    JsElementT(const JsElementT &rhs);

    // these must not be explicit!
    JsElementT(JsStringT<Alloc> &&rhs);
    JsElementT(JsArrayT<Alloc> &&rhs);
    JsElementT(JsObjectT<Alloc> &&rhs);
    JsElementT(JsElementT &&rhs) noexcept;

    ~JsElementT();

    [[nodiscard]] JsType type() const { return type_; }

    [[nodiscard]] JsLiteral &as_lit();
    [[nodiscard]] JsNumber &as_num();
    [[nodiscard]] JsStringT<Alloc> &as_str();
    [[nodiscard]] JsArrayT<Alloc> &as_arr();
    [[nodiscard]] JsObjectT<Alloc> &as_obj();

    [[nodiscard]] const JsLiteral &as_lit() const;
    [[nodiscard]] const JsNumber &as_num() const;
    [[nodiscard]] const JsStringT<Alloc> &as_str() const;
    [[nodiscard]] const JsArrayT<Alloc> &as_arr() const;
    [[nodiscard]] const JsObjectT<Alloc> &as_obj() const;

    JsElementT &operator=(JsElementT &&rhs) noexcept;
    JsElementT &operator=(const JsElementT &rhs);

    bool operator==(const JsElementT &rhs) const;
    bool operator!=(const JsElementT &rhs) const { return !operator==(rhs); }

    [[nodiscard]] bool Equals(const JsElementT &rhs, double eps) const;

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
} // namespace Sys