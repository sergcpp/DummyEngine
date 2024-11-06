#pragma once

#include <climits>

#include <deque>
#include <functional>
#include <istream>
#include <memory>
#include <string_view>
#include <utility>

#include "Span.h"
#include "parser/PoolAlloc.h"

//
// Based on "Tiny C PreProcessor" (https://github.com/bnoazx005/tcpp)
//

namespace glslx {
using string = std::basic_string<char, std::char_traits<char>, MultiPoolAllocator<char>>;

std::unique_ptr<std::istream> default_include_callback(const char *path, bool is_system_path);

struct macro_def_t {
    std::string name;
    int value; // only numbers for now
};

struct preprocessor_config_t {
    bool strip_comments = false;
    int empty_macro_value = INT_MAX;
    std::vector<macro_def_t> default_macros;
    std::function<std::unique_ptr<std::istream>(const char *path, bool is_system_path)> include_callback =
        default_include_callback;
};

class Preprocessor {
    MultiPoolAllocator<char> alloc_;
    std::vector<std::unique_ptr<std::istream>> streams_;
    preprocessor_config_t config_;
    size_t source_line_ = 0;
    size_t curr_pos_ = 0;
    std::string error_;

    enum class eTokenType {
        Unknown,
        Identifier,
        Define,
        If,
        Else,
        Elif,
        Undef,
        Endif,
        Include,
        Defined,
        Ifndef,
        Ifdef,
        Space,
        Blob,
        Bracket_Begin,
        Bracket_End,
        Comma,
        Newline,
        Less,
        Greater,
        Quotes,
        Keyword,
        End,
        Reject_Macro,
        Stringize_Op,
        Concat_Op,
        Number,
        Plus,
        Minus,
        Slash,
        Star,
        Or,
        And,
        Ampersand,
        Vline,
        LShift,
        RShift,
        Not,
        GreaterEqual,
        LessEqual,
        Equal,
        NotEqual,
        Colon,
        Semicolon,
        PassthroughDirective,
        Comment,
        Extension
    };

    struct token_t {
        eTokenType type;

        // std::string raw_view;
        string raw_view;

        size_t line, pos;

        explicit token_t(MultiPoolAllocator<char> &alloc) : raw_view(alloc) {}
        token_t(const eTokenType _type, MultiPoolAllocator<char> &alloc) : type(_type), raw_view(alloc) {}
        token_t(const eTokenType _type, std::string_view _raw_view, MultiPoolAllocator<char> &alloc,
                const size_t _line = 0, const size_t _pos = 0)
            : type(_type), raw_view(_raw_view, alloc), line(_line), pos(_pos) {}
        token_t(const eTokenType _type, const string &_raw_view, const size_t _line = 0, const size_t _pos = 0)
            : type(_type), raw_view(_raw_view), line(_line), pos(_pos) {}
        token_t(const eTokenType _type, string &&_raw_view, const size_t _line = 0, const size_t _pos = 0)
            : type(_type), raw_view(std::move(_raw_view)), line(_line), pos(_pos) {}
        bool operator==(const token_t &rhs) const { return type == rhs.type && raw_view == rhs.raw_view; }
    };

    struct macro_desc_t {
        string name;

        std::vector<string> arg_names;

        std::vector<token_t> value;

        macro_desc_t(std::string_view _name, MultiPoolAllocator<char> &alloc) : name(_name, alloc) {}

        bool operator==(const macro_desc_t &rhs) const {
            return name == rhs.name && arg_names == rhs.arg_names && value == rhs.value;
        }
    };

    struct if_block_t {
        bool should_be_skipped = true;
        bool has_else_been_found = false;
        bool has_if_been_entered = false;
    };

    std::vector<std::pair<string, eTokenType>> directives_table_;
    std::vector<macro_desc_t> macros_;
    std::vector<string> context_stack_;
    std::vector<if_block_t> if_blocks_;

    string current_line_;
    std::deque<token_t> tokens_queue_;
    string temp_str_;

    bool expect(const eTokenType expected_type, const eTokenType actual_type) {
        if (expected_type != actual_type) {
            error_ = "Unexpected token at " + std::to_string(source_line_);
            return false;
        }
        return true;
    }

    void ReadLine(string &out_line);
    void RequestSourceLine(string &out_line);
    void ScanTokens(token_t &out_tok, string &inout_line);
    void ScanSeparator(token_t &out_tok, char ch, string &inout_line);
    void GetNextToken(token_t &out_tok);

    string ExtractSingleLineComment(const string &line);
    string ExtractMultiLineComment(string &line);

    bool CreateMacroDefinition();
    bool RemoveMacroDefinition(std::string_view macro_name);

    bool ProcessInclude();
    bool ProcessExtension(std::string &output);

    bool ProcessIf();
    bool ProcessIfdef();
    bool ProcessIfndef();
    bool ProcessElse();
    bool ProcessElif();

    [[nodiscard]] bool ShouldTokenBeSkipped() const {
        for (int i = int(if_blocks_.size()) - 1; i >= 0; --i) {
            if (if_blocks_[i].should_be_skipped) {
                return true;
            }
        }
        return false;
    }

    std::vector<token_t> ExpandMacroDefinition(const macro_desc_t &macro, const token_t &token,
                                               const std::function<void(token_t &)> &get_next_token);
    int EvaluateExpression(Span<const token_t> tokens);

  public:
    explicit Preprocessor(std::unique_ptr<std::istream> stream, preprocessor_config_t config = {});
    explicit Preprocessor(std::string_view source, preprocessor_config_t config = {});
    ~Preprocessor();

    [[nodiscard]] std::string_view error() const { return error_; }

    [[nodiscard]] std::string Process();

    [[nodiscard]] Span<const macro_desc_t> macros() const {
        return Span<const macro_desc_t>{macros_.data() + 1, macros_.size() - 1};
    }
};
} // namespace glslx