#pragma once

#include <climits>

#include <deque>
#include <functional>
#include <istream>
#include <memory>
#include <string_view>
#include <utility>

#include "Span.h"

//
// Based on "Tiny C PreProcessor" (https://github.com/bnoazx005/tcpp)
//

namespace glslx {
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
    std::vector<std::unique_ptr<std::istream>> streams_;
    preprocessor_config_t config_;
    size_t source_line_ = 0;
    size_t curr_pos_ = 0;
    std::string output_;
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
        Semicolon,
        PassthroughDirective,
        Comment
    };

    struct token_t {
        eTokenType type;

        std::string raw_view;

        size_t line, pos;

        bool operator==(const token_t &rhs) const {
            return type == rhs.type && raw_view == rhs.raw_view;
        }
    };

    struct macro_desc_t {
        std::string name;

        std::vector<std::string> arg_names;

        std::vector<token_t> value;

        bool operator==(const macro_desc_t &rhs) const {
            return name == rhs.name && arg_names == rhs.arg_names && value == rhs.value;
        }
    };

    struct if_block_t {
        bool should_be_skipped = true;
        bool has_else_been_found = false;
        bool has_if_been_entered = false;
    };

    std::vector<std::pair<std::string, eTokenType>> directives_table_;
    std::vector<macro_desc_t> macros_;
    std::vector<std::string> context_stack_;
    std::vector<if_block_t> if_blocks_;

    std::string current_line_;
    std::deque<token_t> tokens_queue_;

    bool expect(const eTokenType expected_type, const eTokenType actual_type) {
        if (expected_type != actual_type) {
            error_ = "Unexpected token at " + std::to_string(source_line_);
            return false;
        }
        return true;
    }

    void ReadLine(std::string &out_line);
    void RequestSourceLine(std::string &out_line);
    token_t ScanTokens(std::string &inout_line);
    token_t ScanSeparator(char ch, std::string &inout_line);
    token_t GetNextToken();

    std::string ExtractSingleLineComment(const std::string &line);
    std::string ExtractMultiLineComment(std::string &line);

    bool CreateMacroDefinition();
    bool RemoveMacroDefinition(const std::string &macro_name);

    bool ProcessInclude();

    bool ProcessIf();
    bool ProcessIfdef();
    bool ProcessIfndef();
    bool ProcessElse();
    bool ProcessElif();

    bool ShouldTokenBeSkipped() const {
        for (int i = int(if_blocks_.size()) - 1; i >= 0; --i) {
            if (if_blocks_[i].should_be_skipped) {
                return true;
            }
        }
        return false;
    }

    std::vector<token_t> ExpandMacroDefinition(const macro_desc_t &macro, const token_t &token,
                                               const std::function<token_t()> &get_next_token);
    int EvaluateExpression(Span<const token_t> tokens);

  public:
    Preprocessor(std::unique_ptr<std::istream> stream, const preprocessor_config_t &config = {});
    Preprocessor(std::string_view source, const preprocessor_config_t &config = {});
    ~Preprocessor();

    std::string_view error() const { return error_; }

    std::string_view Process();

    Span<const macro_desc_t> macros() const { return Span<const macro_desc_t>{macros_.data() + 1, macros_.size() - 1}; }
};
} // namespace glslx