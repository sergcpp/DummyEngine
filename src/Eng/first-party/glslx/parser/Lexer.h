#pragma once

#include <cstdint>

#include <string>

#include "../SmallVector.h"
#include "PoolAlloc.h"

namespace glslx {
template <typename T> using local_vector = SmallVector<T, 1, alignof(T), MultiPoolAllocator<T>>;
template <typename T> using global_vector = SmallVector<T, 1>;

enum class eTokType {
    Eof,
    Whitespace,
    Comment,
    Keyword,
    Identifier,
    Const_short,
    Const_ushort,
    Const_int,
    Const_uint,
    Const_long,
    Const_ulong,
    Const_half,
    Const_float,
    Const_double,
    Operator,
    Semicolon,
    Scope_Begin,
    Scope_End,
    Directive
};

#define X(_0, _1, _2) K_##_0,
enum class eKeyword : uint16_t {
#include "Keywords.inl"
};
#undef X

inline bool operator<(const eKeyword lhs, const eKeyword rhs) {
    return std::underlying_type<eKeyword>::type(lhs) < std::underlying_type<eKeyword>::type(rhs);
}

#define X(_0, _1, _2) _0,
enum class eOperator : uint8_t {
#include "Operators.inl"
};
#undef X

enum class eDirType : uint8_t { Version, Extension };

enum class eVerType : uint8_t { Core, Compatibility, ES };

enum class eExtBehavior : uint8_t { Invalid, Enable, Require, Warn, Disable };

struct directive_t {
    eDirType type;
    union {
        struct {
            eVerType type;
            int number;
        } as_version;
        struct {
            const char *name;
            eExtBehavior behavior;
        } as_extension;
    };
};

struct token_t {
    eTokType type = eTokType::Eof;
    union {
        const char *as_identifier = nullptr;
        int16_t as_short;
        uint16_t as_ushort;
        int32_t as_int;
        uint32_t as_uint;
        int64_t as_long;
        uint64_t as_ulong;
        float as_half;
        float as_float;
        double as_double;
        eKeyword as_keyword;
        eOperator as_operator;
        directive_t as_directive;
    };
    local_vector<char> string_mem;

    explicit token_t(MultiPoolAllocator<char> &_alloc) : string_mem(_alloc) {}
    [[nodiscard]] int precedence() const;
};

struct location_t {
    size_t col = 1;
    size_t line = 1;
    size_t pos = 0;

    void advance(size_t count = 1) {
        col += count;
        pos += count;
    }
    void advance_line() {
        ++line;
        ++pos;
        col = 1;
    }
};

struct keyword_info_t {
    const char *name;
    eKeyword type;
    bool is_typename : 1;
    bool is_reserved : 1;
};
extern const keyword_info_t g_keywords[];

struct operator_info_t {
    const char *name;
    const char *string;
    int precedence;
};
extern const operator_info_t g_operators[];

class Lexer {
    std::string_view source_;
    location_t loc_;
    token_t temp_tok_;
    const char *error_ = nullptr;

    void ReadNumeric(bool octal, bool hex, std::string &out_digits);

    static std::string CalcKeywordsLookupTable();

  public:
    explicit Lexer(MultiPoolAllocator<char> &alloc) : temp_tok_(alloc) {}
    Lexer(MultiPoolAllocator<char> &alloc, std::string_view source);

    [[nodiscard]] location_t location() const { return loc_; }
    [[nodiscard]] size_t position() const { return loc_.pos; }
    [[nodiscard]] size_t line() const { return loc_.line; }
    [[nodiscard]] size_t column() const { return loc_.col; }

    void set_location(const location_t &loc) { loc_ = loc; }

    [[nodiscard]] int at(int offset = 0) const;

    [[nodiscard]] const char *error() const { return error_; }

    void ReadSingle(token_t &out);
    void Read(token_t &out, const bool skip_whitespace) {
        do {
            ReadSingle(out);
        } while (skip_whitespace && (out.type == eTokType::Whitespace || out.type == eTokType::Comment) && !error_);
    }
    const token_t &Peek() {
        const location_t loc_before = loc_;
        Read(temp_tok_, true);
        loc_ = loc_before;
        return temp_tok_;
    }

    void SkipWhitespace(bool skip_newline = false);
};

inline int Lexer::at(const int offset) const {
    if (position() + offset < size_t(source_.size())) {
        return source_[position() + offset];
    }
    return 0;
}
} // namespace glslx