#include "Lexer.h"

#include <climits>

namespace glslx {
inline bool is_octal(const int ch) { return unsigned(ch) - '0' < 8; }
inline bool is_hex(const int ch) { return (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F') || isdigit(ch); }
inline bool is_space(const int ch) { return (ch >= '\t' && ch <= '\r') || ch == ' '; }

#define X(_0, _1, _2) {#_0, eKeyword::K_##_0, _1, _2},
extern const keyword_info_t g_keywords[] = {
#include "Keywords.inl"
};
#undef X

#define X(_0, _1, _2) {#_0, _1, _2},
extern const operator_info_t g_operators[] = {
#include "Operators.inl"
};
#undef X
} // namespace glslx

int glslx::token_t::precedence() const {
    // TODO: change precedence direction
    if (type == eTokType::Operator) {
        return 18 - g_operators[int(as_operator)].precedence;
    }
    return -1;
}

glslx::Lexer::Lexer(MultiPoolAllocator<char> &alloc, std::string_view source) : source_(source), temp_tok_(alloc) {}

void glslx::Lexer::ReadSingle(token_t &out) {
    out.string_mem.clear();

    if (position() == source_.size()) {
        out.type = eTokType::Eof;
        return;
    }

    if (isdigit(at()) || (at() == '.' && isdigit(at(1)))) {
        bool is_half = false;
        bool is_float = false;
        bool is_double = false;
        bool is_short = false;
        bool is_long = false;
        bool is_unsigned = false;
        bool is_octal = false;
        bool is_hex = false;

        if (at() == '0') {
            if (at(1) && (at(1) == 'x' || at(1) == 'X')) {
                is_hex = true;
                loc_.advance(2); // skip '0x'
            } else {
                is_octal = true;
            }
        }

        std::string numeric;
        ReadNumeric(is_octal, is_hex, numeric);
        if (position() != source_.size() && at() == '.') {
            is_float = true;
            is_octal = false;
            numeric.push_back('.');
            loc_.advance();
            ReadNumeric(is_octal, is_hex, numeric);
        }

        if (position() != source_.size() && (at() == 'e' || at() == 'E')) {
            if (isdigit(at(1))) {
                numeric.push_back(at(0));
                numeric.push_back(at(1));
                loc_.advance(2);
                ReadNumeric(is_octal, is_hex, numeric);
                is_float = true;
                is_octal = false;
            } else if ((at(1) == '+' || at(1) == '-') && isdigit(at(2))) {
                numeric.push_back(at(0));
                numeric.push_back(at(1));
                numeric.push_back(at(2));
                loc_.advance(3);
                ReadNumeric(is_octal, is_hex, numeric);
                is_float = true;
                is_octal = false;
            } else {
                error_ = "Invalid numeric literal";
                return;
            }
        }

        if (position() != source_.size() && isalpha(at())) {
            if ((at() == 'h' && at(1) == 'f') || (at() == 'H' && at(1) == 'F')) {
                is_half = true;
                is_octal = false;
                loc_.advance();
            } else if (at() == 'f' || at() == 'F') {
                is_float = true;
                is_octal = false;
            } else if ((at() == 'l' && at(1) == 'f') || (at() == 'L' && at(1) == 'F')) {
                is_float = false;
                is_double = true;
                is_octal = false;
                loc_.advance();
            } else if (at() == 's' || at() == 'S') {
                if (is_float) {
                    error_ = "Invalid use of suffix on literal";
                    return;
                }
                is_short = true;
            } else if ((at() == 'u' && at(1) == 's') || (at() == 'U' && at(1) == 'S')) {
                if (is_float) {
                    error_ = "Invalid use of suffix on literal";
                    return;
                }
                is_short = true;
                is_unsigned = true;
                loc_.advance();
            } else if ((at() == 'u' && at(1) == 'l') || (at() == 'U' && at(1) == 'L')) {
                if (is_float) {
                    error_ = "Invalid use of suffix on literal";
                    return;
                }
                is_long = true;
                is_unsigned = true;
                loc_.advance();
            } else if (at() == 'u' || at() == 'U') {
                if (is_float) {
                    error_ = "Invalid use of suffix on literal";
                    return;
                }
                is_unsigned = true;
            } else if (at() == 'l' || at() == 'L') {
                if (is_float) {
                    error_ = "Invalid use of suffix on literal";
                    return;
                }
                is_long = true;
            } else {
                error_ = "Invalid numeric literal";
                return;
            }
            loc_.advance();
        }

        if (is_hex && (is_half || is_float || is_double)) {
            error_ = "Invalid numeric literal";
            return;
        }

        const int base = is_hex ? 16 : (is_octal ? 8 : 10);
        if (is_half) {
            out.type = eTokType::Const_half;
            char *end;
            out.as_half = strtof(numeric.data(), &end);
            if (end == numeric.data()) {
                error_ = "Invalid numeric literal";
                return;
            }
        } else if (is_float) {
            out.type = eTokType::Const_float;
            char *end;
            out.as_float = strtof(numeric.data(), &end);
            if (end == numeric.data()) {
                error_ = "Invalid numeric literal";
                return;
            }
        } else if (is_double) {
            out.type = eTokType::Const_double;
            char *end;
            out.as_double = strtod(numeric.data(), &end);
            if (end == numeric.data()) {
                error_ = "Invalid numeric literal";
                return;
            }
        } else if (is_unsigned) {
            if (is_short) {
                out.type = eTokType::Const_ushort;
                const unsigned long value = strtoul(numeric.data(), nullptr, base);
                if (value <= USHRT_MAX) {
                    out.as_ushort = uint16_t(value);
                } else {
                    error_ = "Literal needs more than 16-bits";
                }
            } else if (is_long) {
                out.type = eTokType::Const_ulong;
                const unsigned long long value = strtoul(numeric.data(), nullptr, base);
                // TODO: Check if it actually fits
                out.as_ulong = uint64_t(value);
            } else {
                out.type = eTokType::Const_uint;
                const unsigned long long value = strtoull(numeric.data(), nullptr, base);
                if (value <= UINT_MAX) {
                    out.as_uint = uint32_t(value);
                } else {
                    error_ = "Literal needs more than 32-bits";
                }
            }
        } else {
            if (is_short) {
                out.type = eTokType::Const_short;
                const long value = strtol(numeric.data(), nullptr, base);
                if (value >= SHRT_MIN && value <= SHRT_MAX) {
                    out.as_short = int16_t(value);
                } else {
                    error_ = "Literal needs more than 16-bits";
                }
            } else if (is_long) {
                out.type = eTokType::Const_long;
                const long long value = strtoll(numeric.data(), nullptr, base);
                // TODO: Check if it actually fits
                out.as_long = int64_t(value);
            } else {
                out.type = eTokType::Const_int;
                const long long value = strtoll(numeric.data(), nullptr, base);
                if (value >= INT_MIN && value <= INT_MAX) {
                    out.as_int = int32_t(value);
                } else {
                    const unsigned long long uvalue = strtoull(numeric.data(), nullptr, base);
                    if (value <= UINT_MAX) {
                        out.as_int = int32_t(uvalue);
                    } else {
                        error_ = "Literal needs more than 32-bits";
                    }
                }
            }
        }
    } else if (isalpha(at()) || at() == '_') {
        out.type = eTokType::Identifier;
        while (position() != source_.size() && (isalpha(at()) || isdigit(at()) || at() == '_')) {
            out.string_mem.push_back(at());
            loc_.advance();
        }
        out.string_mem.push_back('\0');
        out.as_identifier = out.string_mem.data();

        for (int i = 0; i < std::size(g_keywords); ++i) {
            if (strcmp(g_keywords[i].name, out.as_identifier) == 0) {
                out.type = eTokType::Keyword;
                out.as_keyword = g_keywords[i].type;
                break;
            }
        }
    } else if (at() == '#') {
        loc_.advance(); // skip '#'

        std::string chars;
        while (isalpha(at())) {
            chars.push_back(at());
            loc_.advance();
        }

        if (chars.empty()) {
            error_ = "Expected directive";
            return;
        }

        if (chars == "version") {
            out.type = eTokType::Directive;
            out.as_directive.type = eDirType::Version;

            // version [0-9]+ (core|compatibility|es)?
            SkipWhitespace();

            std::string digits;
            ReadNumeric(false, false, digits);
            if (digits.empty()) {
                error_ = "Expected version number in #version directive";
                return;
            }

            const long long value = strtoll(digits.data(), 0, 10);
            out.as_directive.as_version.number = int(value);
            out.as_directive.as_version.type = eVerType::Core;

            SkipWhitespace();

            std::string chars;
            while (isalpha(at())) {
                chars.push_back(at());
                loc_.advance();
            }

            if (!chars.empty()) {
                if (chars == "core") {
                } else if (chars == "compatibility") {
                    out.as_directive.as_version.type = eVerType::Compatibility;
                } else if (chars == "es") {
                    out.as_directive.as_version.type = eVerType::ES;
                } else {
                    error_ = "Invalid profile in #version directive";
                    return;
                }
            }
        } else if (chars == "extension") {
            out.type = eTokType::Directive;
            out.as_directive.type = eDirType::Extension;

            // extension [a-zA-Z_]+ : (enable|require|warn|disable)
            SkipWhitespace();

            std::string extension;
            while (isalpha(at()) || isdigit(at()) || at() == '_') {
                extension.push_back(at());
                loc_.advance();
            }
            if (extension.empty()) {
                error_ = "Expected extension name in #extension directive";
                return;
            }

            SkipWhitespace();

            if (at() != ':') {
                error_ = "Expected ':' in #extension directive";
                return;
            }

            loc_.advance(); // Skip ':'

            SkipWhitespace();

            std::string behavior;
            while (isalpha(at())) {
                behavior.push_back(at());
                loc_.advance();
            }
            if (behavior.empty()) {
                error_ = "Expected behavior in #extension directive";
            }

            if (behavior == "enable") {
                out.as_directive.as_extension.behavior = eExtBehavior::Enable;
            } else if (behavior == "require") {
                out.as_directive.as_extension.behavior = eExtBehavior::Require;
            } else if (behavior == "warn") {
                out.as_directive.as_extension.behavior = eExtBehavior::Warn;
            } else if (behavior == "disable") {
                out.as_directive.as_extension.behavior = eExtBehavior::Disable;
            } else {
                error_ = "Unexpected behavior in #extension directive";
                return;
            }

            out.string_mem.assign(extension.data(), extension.data() + extension.length() + 1);
            out.as_directive.as_extension.name = out.string_mem.data();
        } else if (chars == "line") {
            SkipWhitespace();

            std::string line;
            ReadNumeric(false, false, line);
            if (line.empty()) {
                error_ = "Empty #line directive";
                return;
            }

            loc_.line = strtoull(line.data(), nullptr, 10);
        } else if (chars == "error") {
            SkipWhitespace();

            std::string error;
            while (isalpha(at()) || isdigit(at()) || at() == '"') {
                error.push_back(at());
                loc_.advance();
            }
            if (error.empty()) {
                error_ = "Empty #error directive";
                return;
            }

            out.string_mem.assign(error.data(), error.data() + error.length() + 1);
            error_ = out.string_mem.data();
        } else {
            error_ = "Unsupported directive";
            return;
        }
    } else {
        switch (at()) {
        case '\n':
        case '\t':
        case '\f':
        case '\v':
        case '\r':
        case ' ':
            SkipWhitespace(true /* skip_newline */);
            out.type = eTokType::Whitespace;
            break;
        case ';':
            out.type = eTokType::Semicolon;
            loc_.advance();
            break;
        case '{':
            out.type = eTokType::Scope_Begin;
            loc_.advance();
            break;
        case '}':
            out.type = eTokType::Scope_End;
            loc_.advance();
            break;
        // operators (https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#operators)
        case '.':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::dot;
            break;
        case '+':
            out.type = eTokType::Operator;
            if (at(1) == '+') {
                out.as_operator = eOperator::increment;
            } else if (at(1) == '=') {
                out.as_operator = eOperator::add_assign;
            } else {
                out.as_operator = eOperator::plus;
            }
            break;
        case '-':
            out.type = eTokType::Operator;
            if (at(1) == '-') {
                out.as_operator = eOperator::decrement;
            } else if (at(1) == '=') {
                out.as_operator = eOperator::sub_assign;
            } else {
                out.as_operator = eOperator::minus;
            }
            break;
        case '/':
            if (at(1) == '/') {
                // line comments
                while (position() != source_.size()) {
                    if (at() == '\n') {
                        loc_.advance_line();
                        break;
                    }
                    loc_.advance();
                }
                out.type = eTokType::Comment;
            } else if (at(1) == '*') {
                // block comments
                while (position() != source_.size()) {
                    if (at() == '\n') {
                        loc_.advance_line();
                        continue;
                    }
                    if (at() == '*' && at(1) == '/') {
                        loc_.advance(2);
                        break;
                    }
                    loc_.advance();
                }
                out.type = eTokType::Comment;
            } else if (at(1) == '=') {
                out.type = eTokType::Operator;
                out.as_operator = eOperator::divide_assign;
            } else {
                out.type = eTokType::Operator;
                out.as_operator = eOperator::divide;
            }
            break;
        case '*':
            out.type = eTokType::Operator;
            if (at(1) == '=') {
                out.as_operator = eOperator::multiply_assign;
            } else {
                out.as_operator = eOperator::multiply;
            }
            break;
        case '%':
            out.type = eTokType::Operator;
            if (at(1) == '=') {
                out.as_operator = eOperator::modulus_assign;
            } else {
                out.as_operator = eOperator::modulus;
            }
            break;
        case '<':
            out.type = eTokType::Operator;
            if (at(1) == '<' && at(2) == '=') {
                out.as_operator = eOperator::shift_left_assign;
            } else if (at(1) == '<') {
                out.as_operator = eOperator::shift_left;
            } else if (at(1) == '=') {
                out.as_operator = eOperator::less_equal;
            } else {
                out.as_operator = eOperator::less;
            }
            break;
        case '>':
            out.type = eTokType::Operator;
            if (at(1) == '>' && at(2) == '=') {
                out.as_operator = eOperator::shift_right_assign;
            } else if (at(1) == '>') {
                out.as_operator = eOperator::shift_right;
            } else if (at(1) == '=') {
                out.as_operator = eOperator::greater_equal;
            } else {
                out.as_operator = eOperator::greater;
            }
            break;
        case '[':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::bracket_begin;
            break;
        case ']':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::bracket_end;
            break;
        case '(':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::parenthesis_begin;
            break;
        case ')':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::parenthesis_end;
            break;
        case '^':
            out.type = eTokType::Operator;
            if (at(1) == '^') {
                out.as_operator = eOperator::logical_xor;
            } else if (at(1) == '=') {
                out.as_operator = eOperator::bit_xor_assign;
            } else {
                out.as_operator = eOperator::bit_xor;
            }
            break;
        case '|':
            out.type = eTokType::Operator;
            if (at(1) == '|') {
                out.as_operator = eOperator::logical_or;
            } else if (at(1) == '=') {
                out.as_operator = eOperator::bit_or_assign;
            } else {
                out.as_operator = eOperator::bit_or;
            }
            break;
        case '&':
            out.type = eTokType::Operator;
            if (at(1) == '&') {
                out.as_operator = eOperator::logical_and;
            } else if (at(1) == '=') {
                out.as_operator = eOperator::bit_and_assign;
            } else {
                out.as_operator = eOperator::bit_and;
            }
            break;
        case '~':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::bit_not;
            break;
        case '=':
            out.type = eTokType::Operator;
            if (at(1) == '=') {
                out.as_operator = eOperator::equal;
            } else {
                out.as_operator = eOperator::assign;
            }
            break;
        case '!':
            out.type = eTokType::Operator;
            if (at(1) == '=') {
                out.as_operator = eOperator::not_equal;
            } else {
                out.as_operator = eOperator::logital_not;
            }
            break;
        case ':':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::colon;
            break;
        case ',':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::comma;
            break;
        case '?':
            out.type = eTokType::Operator;
            out.as_operator = eOperator::questionmark;
            break;
        default:
            error_ = "Invalid character encountered";
            return;
        }
        if (out.type == eTokType::Operator) {
            loc_.advance(strlen(g_operators[int(out.as_operator)].string));
        }
    }
}

void glslx::Lexer::SkipWhitespace(const bool skip_newline) {
    while (position() < size_t(source_.size()) && is_space(at())) {
        if (at() == '\n') {
            if (skip_newline) {
                loc_.advance_line();
            } else {
                break;
            }
        } else {
            loc_.advance();
        }
    }
}

void glslx::Lexer::ReadNumeric(const bool octal, const bool hex, std::string &out_digits) {
    if (octal) {
        while (position() < size_t(source_.size()) && is_octal(at())) {
            out_digits.push_back(at());
            loc_.advance();
        }
    } else if (hex) {
        while (position() < size_t(source_.size()) && is_hex(at())) {
            out_digits.push_back(at());
            loc_.advance();
        }
    } else {
        while (position() < size_t(source_.size()) && isdigit(at())) {
            out_digits.push_back(at());
            loc_.advance();
        }
    }
}
