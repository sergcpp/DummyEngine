#include "Preprocessor.h"

#include <fstream>
#include <sstream>

namespace glslx {
bool is_escape_sequence(const std::string &str, size_t pos) {
    if (pos + 1 >= str.length() || pos >= str.length() || str[pos] != '\\') {
        return false;
    }

    static const char EscapeSymbols[] = {'\'', '\\', 'n', '\"', 'a', 'b', 'f', 'r', 't', 'v'};

    char test_symbol = str[pos + 1];

    for (const char ch : EscapeSymbols) {
        if (ch == test_symbol) {
            return true;
        }
    }

    return false;
}
bool is_separator(const char ch) {
    static const char Separators[] = ",()<>\"+-*/&|!=;";
    for (int i = 0; i < sizeof(Separators) - 1; ++i) {
        if (Separators[i] == ch) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<std::istream> default_include_callback(const char *path, bool is_system_path) {
    return std::make_unique<std::ifstream>(path, std::ios::binary);
}
} // namespace glslx

glslx::Preprocessor::Preprocessor(std::unique_ptr<std::istream> stream, const preprocessor_config_t &config)
    : config_(config), directives_table_{{"define", eTokenType::Define},
                                         {"ifdef", eTokenType::Ifdef},
                                         {"ifndef", eTokenType::Ifndef},
                                         {"if", eTokenType::If},
                                         {"else", eTokenType::Else},
                                         {"elif", eTokenType::Elif},
                                         {"undef", eTokenType::Undef},
                                         {"endif", eTokenType::Endif},
                                         {"include", eTokenType::Include},
                                         {"defined", eTokenType::Defined},
                                         {"line", eTokenType::PassthroughDirective},
                                         {"version", eTokenType::PassthroughDirective},
                                         {"extension", eTokenType::PassthroughDirective},
                                         {"pragma", eTokenType::PassthroughDirective}},
      macros_{{"__LINE__"}} {
    streams_.push_back(std::move(stream));
    for (const macro_def_t &m : config_.default_macros) {
        macros_.push_back({m.name});
        if (m.value != config_.empty_macro_value) {
            macros_.back().value.push_back({eTokenType::Number, std::to_string(m.value)});
        }
    }
}

glslx::Preprocessor::Preprocessor(std::string_view source, const preprocessor_config_t &config)
    : Preprocessor(std::make_unique<std::istringstream>(source.data()), config) {}

glslx::Preprocessor::~Preprocessor() = default;

std::string glslx::Preprocessor::Process() {
    std::string output;

    token_t curr_token = GetNextToken();
    while (curr_token.type != eTokenType::End) {
        switch (curr_token.type) {
        case eTokenType::Define:
            if (!CreateMacroDefinition()) {
                return {};
            }
            break;
        case eTokenType::Undef:
            curr_token = GetNextToken();
            if (!expect(eTokenType::Space, curr_token.type)) {
                return {};
            }

            curr_token = GetNextToken();
            if (!expect(eTokenType::Identifier, curr_token.type)) {
                return {};
            }

            if (!RemoveMacroDefinition(curr_token.raw_view)) {
                return {};
            }
            break;
        case eTokenType::If:
            if (!ProcessIf()) {
                return {};
            }
            break;
        case eTokenType::Ifdef:
            if (!ProcessIfdef()) {
                return {};
            }
            break;
        case eTokenType::Ifndef:
            if (!ProcessIfndef()) {
                return {};
            }
            break;
        case eTokenType::Elif:
            if (!ProcessElif()) {
                return {};
            }
            break;
        case eTokenType::Else:
            if (!ProcessElse()) {
                return {};
            }
            break;
        case eTokenType::Endif:
            if (if_blocks_.empty()) {
                error_ = "Unbalanced #endif " + std::to_string(source_line_);
                return {};
            } else {
                if_blocks_.pop_back();
            }
            break;
        case eTokenType::Include:
            if (!ProcessInclude()) {
                return {};
            }
            break;
        case eTokenType::Identifier: {
            const auto macro_it =
                std::find_if(cbegin(macros_), cend(macros_),
                             [&curr_token](const macro_desc_t &macro) { return macro.name == curr_token.raw_view; });
            const auto ctx_it =
                std::find_if(cbegin(context_stack_), cend(context_stack_),
                             [&curr_token](const std::string &name) { return name == curr_token.raw_view; });
            if (macro_it != cend(macros_) /*&& ctx_it == cend(context_stack_)*/) {
                const std::vector<token_t> expanded =
                    ExpandMacroDefinition(*macro_it, curr_token, std::bind(&Preprocessor::GetNextToken, this));
                if (expanded.empty() && !error_.empty()) {
                    return {};
                }
                tokens_queue_.insert(begin(tokens_queue_), begin(expanded), end(expanded));
            } else {
                if (!ShouldTokenBeSkipped()) {
                    output.append(curr_token.raw_view);
                }
            }
        } break;
        case eTokenType::Reject_Macro:
            context_stack_.erase(
                std::remove_if(begin(context_stack_), end(context_stack_),
                               [&curr_token](const std::string &name) { return name == curr_token.raw_view; }),
                end(context_stack_));
            break;
        case eTokenType::Concat_Op:
            while (output.back() == ' ') {
                output.pop_back();
            }

            while ((curr_token = GetNextToken()).type == eTokenType::Space)
                ; // skip space tokens

            if (!ShouldTokenBeSkipped()) {
                output.append(curr_token.raw_view);
            }
            break;
        case eTokenType::Stringize_Op:
            output.append((curr_token = GetNextToken()).raw_view);
            break;
        case eTokenType::PassthroughDirective:
            if (!ShouldTokenBeSkipped()) {
                output.append("#");
                output.append(curr_token.raw_view);
            }
            break;
        default:
            if (curr_token.type == eTokenType::Comment && config_.strip_comments) {
                break;
            }
            if (!ShouldTokenBeSkipped()) {
                output.append(curr_token.raw_view);
            }
        }
        curr_token = GetNextToken();
        if (curr_token.type == eTokenType::End && !streams_.empty()) {
            streams_.pop_back();
            curr_token = GetNextToken();
        }
    }
    return output;
}

void glslx::Preprocessor::ReadLine(std::string &out_line) {
    if (streams_.empty() || streams_.back()->eof() || !streams_.back()->good()) {
        out_line.clear();
        return;
    }

    std::getline(*streams_.back(), out_line, '\n');
    if (!streams_.back()->eof()) {
        out_line.push_back('\n');
    }
    ++source_line_;
}

void glslx::Preprocessor::RequestSourceLine(std::string &out_line) {
    ReadLine(out_line);
    size_t pos = 0;
    while ((pos = out_line.find_first_of('\\')) != std::string::npos &&
           (pos >= out_line.length() || std::isspace(out_line[pos + 1])) && !is_escape_sequence(out_line, pos)) {
        std::string next_line;
        ReadLine(next_line);
        if (!next_line.empty()) {
            out_line.replace(pos ? (pos - 1) : 0, std::string::npos, next_line);
        } else {
            out_line.erase(begin(out_line) + pos, end(out_line));
        }
    }
}

glslx::Preprocessor::token_t glslx::Preprocessor::GetNextToken() {
    if (!tokens_queue_.empty()) {
        token_t ret = tokens_queue_.front();
        tokens_queue_.pop_front();
        return ret;
    }

    if (current_line_.empty()) {
        RequestSourceLine(current_line_);
        if (current_line_.empty()) {
            return token_t{eTokenType::End};
        }
    }

    return ScanTokens(current_line_);
}

glslx::Preprocessor::token_t glslx::Preprocessor::ScanTokens(std::string &inout_line) {
    std::string curr_str;

    char ch = '\0';
    while (!inout_line.empty()) {
        ch = inout_line.front();
        if (ch == '/') {
            std::string comment_str;
            if (inout_line.length() > 1 && inout_line[1] == '/') { // single line comment
                comment_str = ExtractSingleLineComment(inout_line);
            } else if (inout_line.length() > 1 && inout_line[1] == '*') { // multi-line comment
                comment_str = ExtractMultiLineComment(inout_line);
            }

            if (!comment_str.empty()) {
                inout_line.erase(0, comment_str.length());
                curr_pos_ += comment_str.length();
                return {eTokenType::Comment, comment_str, source_line_, curr_pos_};
            }
        }

        if (ch == '\n' || ch == '\r') {
            if (!curr_str.empty()) {
                return {eTokenType::Blob, curr_str, source_line_};
            }

            std::string separator;
            separator.push_back(inout_line.front());

            const char next = inout_line[1];
            if (ch == '\r' && next == '\n') {
                separator.push_back(next);
            }

            inout_line.erase(0, separator.length());
            curr_pos_ += separator.length();

            return {eTokenType::Newline, separator, source_line_, curr_pos_};
        }

        if (isspace(ch)) {
            if (!curr_str.empty()) {
                return {eTokenType::Blob, curr_str, source_line_};
            }

            std::string separator;
            separator.push_back(inout_line.front());

            inout_line.erase(0, 1);
            ++curr_pos_;

            return {eTokenType::Space, separator, source_line_, curr_pos_};
        }

        if (ch == '#') { // operator or directive
            if (!curr_str.empty()) {
                return {eTokenType::Blob, curr_str, source_line_};
            }

            do {
                inout_line.erase(0, 1);
                ++curr_pos_;
            } while (isspace(inout_line.front()));

            for (const auto &dir : directives_table_) {
                if (inout_line.rfind(dir.first, 0) == 0) {
                    inout_line.erase(0, dir.first.length());
                    curr_pos_ += dir.first.length();
                    return {dir.second, dir.first, source_line_, curr_pos_};
                }
            }

            if (!inout_line.empty()) { // operator
                const char next = inout_line.front();
                if (next == '#') { // concatenation operator
                    inout_line.erase(0, 1);
                    ++curr_pos_;
                    return {eTokenType::Concat_Op, {}, source_line_, curr_pos_};
                } else if (next != ' ') { // stringification operator
                    return {eTokenType::Stringize_Op, {}, source_line_, curr_pos_};
                } else {
                    return {eTokenType::Blob, "#", source_line_, curr_pos_};
                }
            }
        }

        if (isdigit(ch)) { // number
            if (!curr_str.empty()) {
                return {eTokenType::Blob, curr_str, source_line_};
            }

            std::string number;

            if (ch == '0' && !inout_line.empty()) {
                inout_line.erase(0, 1);
                ++curr_pos_;

                number.push_back(ch);

                const char next = inout_line.front();
                if (next == 'x' || isdigit(next)) {
                    inout_line.erase(0, 1);
                    ++curr_pos_;

                    number.push_back(next);
                } else {
                    return {eTokenType::Number, number, source_line_, curr_pos_};
                }
            }

            int i = 0;
            while ((i < inout_line.length()) && isdigit(ch = inout_line[i++])) {
                number.push_back(ch);
            }

            inout_line.erase(0, i - 1);
            curr_pos_ += i - 1;

            return {eTokenType::Number, number, source_line_, curr_pos_};
        }

        if (isalpha(ch) || ch == '_') { // idendifier
            if (!curr_str.empty()) {
                return {eTokenType::Blob, curr_str, source_line_};
            }

            std::string identifier;

            do {
                identifier.push_back(ch);
                inout_line.erase(0, 1);
                ++curr_pos_;
            } while (!inout_line.empty() && (isalnum(ch = inout_line.front()) || (ch == '_')));

            return {eTokenType::Identifier, identifier, source_line_, curr_pos_};
        }

        inout_line.erase(0, 1);

        if (is_separator(ch)) {
            if (!curr_str.empty()) {
                token_t separator_token = ScanSeparator(ch, inout_line);
                if (separator_token.type != eTokenType::End) {
                    tokens_queue_.push_front(std::move(separator_token));
                }
                return {eTokenType::Blob, curr_str, source_line_, curr_pos_};
            }
            return ScanSeparator(ch, inout_line);
        }

        curr_str.push_back(ch);
    }

    if (!curr_str.empty()) {
        return {eTokenType::Blob, curr_str, source_line_, curr_pos_};
    }

    if (!streams_.empty()) {
        streams_.pop_back();
        return GetNextToken();
    }

    return {eTokenType::End};
}

glslx::Preprocessor::token_t glslx::Preprocessor::ScanSeparator(const char ch, std::string &inout_line) {
    switch (ch) {
    case ',':
        return {eTokenType::Comma, ",", source_line_, curr_pos_};
    case '(':
        return {eTokenType::Bracket_Begin, "(", source_line_, curr_pos_};
    case ')':
        return {eTokenType::Bracket_End, ")", source_line_, curr_pos_};
    case '<':
        if (!inout_line.empty()) {
            const char next = inout_line.front();
            if (next == '<') {
                inout_line.erase(0, 1);
                ++curr_pos_;
                return {eTokenType::LShift, "<<", source_line_, curr_pos_};
            } else if (next == '=') {
                inout_line.erase(0, 1);
                ++curr_pos_;
                return {eTokenType::LessEqual, "<=", source_line_, curr_pos_};
            }
        }
        return {eTokenType::Less, "<", source_line_, curr_pos_};
    case '>':
        if (!inout_line.empty()) {
            const char next = inout_line.front();
            if (next == '>') {
                inout_line.erase(0, 1);
                ++curr_pos_;
                return {eTokenType::RShift, ">>", source_line_, curr_pos_};
            } else if (next == '=') {
                inout_line.erase(0, 1);
                ++curr_pos_;
                return {eTokenType::GreaterEqual, ">=", source_line_, curr_pos_};
            }
        }
        return {eTokenType::Greater, ">", source_line_, curr_pos_};
    case '\"':
        return {eTokenType::Quotes, "\"", source_line_, curr_pos_};
    case '+':
        return {eTokenType::Plus, "+", source_line_, curr_pos_};
    case '-':
        return {eTokenType::Minus, "-", source_line_, curr_pos_};
    case '*':
        return {eTokenType::Star, "*", source_line_, curr_pos_};
    case '/':
        return {eTokenType::Slash, "/", source_line_, curr_pos_};
    case '&':
        if (!inout_line.empty() && inout_line.front() == '&') {
            inout_line.erase(0, 1);
            ++curr_pos_;
            return {eTokenType::And, "&&", source_line_, curr_pos_};
        }
        return {eTokenType::Ampersand, "&", source_line_, curr_pos_};
    case '|':
        if (!inout_line.empty() && inout_line.front() == '|') {
            inout_line.erase(0, 1);
            ++curr_pos_;
            return {eTokenType::Or, "||", source_line_, curr_pos_};
        }
        return {eTokenType::Vline, "|", source_line_, curr_pos_};
    case '!':
        if (!inout_line.empty() && inout_line.front() == '=') {
            inout_line.erase(0, 1);
            ++curr_pos_;
            return {eTokenType::NotEqual, "!=", source_line_, curr_pos_};
        }
        return {eTokenType::Not, "!", source_line_, curr_pos_};
    case '=':
        if (!inout_line.empty() && inout_line.front() == '=') {
            inout_line.erase(0, 1);
            ++curr_pos_;
            return {eTokenType::Equal, "==", source_line_, curr_pos_};
        }
        return {eTokenType::Blob, "=", source_line_, curr_pos_};
    case ';':
        return {eTokenType::Semicolon, ";", source_line_, curr_pos_};
    }
    return {eTokenType::End};
}

std::string glslx::Preprocessor::ExtractSingleLineComment(const std::string &line) {
    std::string ret = line;
    while (ret.back() == '\n') {
        ret.pop_back();
    }
    return ret;
}

std::string glslx::Preprocessor::ExtractMultiLineComment(std::string &line) {
    std::string ret;

    std::string input = line;
    size_t pos;
    while ((pos = input.find("*/")) == std::string::npos && !input.empty()) {
        ret += input;
        RequestSourceLine(input);
        line += input;
    }
    ret += input.substr(0, pos + 2);

    return ret;
}

bool glslx::Preprocessor::CreateMacroDefinition() {
    token_t curr_token = GetNextToken();
    if (!expect(eTokenType::Space, curr_token.type)) {
        return false;
    }

    curr_token = GetNextToken();
    if (!expect(eTokenType::Identifier, curr_token.type)) {
        return false;
    }

    macro_desc_t macro_desc;
    macro_desc.name = curr_token.raw_view;

    auto extract_value = [this](std::vector<token_t> &value) {
        token_t curr_token;
        while ((curr_token = GetNextToken()).type == eTokenType::Space)
            ; // skip space tokens

        while (curr_token.type != eTokenType::Newline) {
            if (curr_token.type != eTokenType::Comment) {
                value.push_back(curr_token);
            }
            curr_token = GetNextToken();
        }

        if (value.empty() && config_.empty_macro_value != INT_MAX) {
            value.push_back({eTokenType::Number, std::to_string(config_.empty_macro_value), source_line_});
        }

        if (!expect(eTokenType::Newline, curr_token.type)) {
            return false;
        }
        return true;
    };

    curr_token = GetNextToken();
    switch (curr_token.type) {
    case eTokenType::Space:
        if (!extract_value(macro_desc.value)) {
            return false;
        }
        break;
    case eTokenType::Newline:
    case eTokenType::End:
        if (config_.empty_macro_value != INT_MAX) {
            macro_desc.value.push_back({eTokenType::Number, std::to_string(config_.empty_macro_value), source_line_});
        }
        break;
    case eTokenType::Bracket_Begin: {
        while (true) {
            while ((curr_token = GetNextToken()).type == eTokenType::Space)
                ; // skip space tokens

            if (!expect(eTokenType::Identifier, curr_token.type)) {
                return false;
            }
            macro_desc.arg_names.push_back(curr_token.raw_view);

            while ((curr_token = GetNextToken()).type == eTokenType::Space)
                ; // skip space tokens
            if (curr_token.type == eTokenType::Bracket_End) {
                break;
            }

            if (!expect(eTokenType::Comma, curr_token.type)) {
                return false;
            }
        }

        if (!extract_value(macro_desc.value)) {
            return false;
        }
    } break;
    default:
        error_ = "Invalid macro definition " + std::to_string(source_line_);
        return false;
    }

    if (ShouldTokenBeSkipped()) {
        return true;
    }

    auto it = std::find_if(begin(macros_), end(macros_),
                           [&macro_desc](const macro_desc_t &desc) { return desc.name == macro_desc.name; });
    if (it != end(macros_)) {
        if (*it == macro_desc) {
            // Same macro definition is allowed
            return true;
        }
        error_ = "Macro '" + macro_desc.name + "' has already been defined " + std::to_string(source_line_);
        return false;
    }

    macros_.push_back(macro_desc);

    return true;
}

bool glslx::Preprocessor::RemoveMacroDefinition(const std::string &macro_name) {
    if (ShouldTokenBeSkipped()) {
        return true;
    }

    auto it = std::find_if(cbegin(macros_), cend(macros_),
                           [&macro_name](const macro_desc_t &macro) { return macro.name == macro_name; });
    if (it == cend(macros_)) {
        return true;
    }

    macros_.erase(it);

    const token_t curr_token = GetNextToken();
    if (!expect(eTokenType::Newline, curr_token.type)) {
        return false;
    }

    return true;
}

bool glslx::Preprocessor::ProcessInclude() {
    if (ShouldTokenBeSkipped()) {
        return true;
    }

    token_t curr_token;
    while ((curr_token = GetNextToken()).type == eTokenType::Space)
        ; // skip space tokens

    if (curr_token.type != eTokenType::Less && curr_token.type != eTokenType::Quotes) {
        while ((curr_token = GetNextToken()).type == eTokenType::Newline)
            ; // skip to the end of current line

        error_ = "Invalid include directive " + std::to_string(source_line_);
        return false;
    }

    const bool is_system_path = (curr_token.type == eTokenType::Less);

    std::string path;

    while (true) {
        if ((curr_token = GetNextToken()).type == eTokenType::Quotes || curr_token.type == eTokenType::Greater) {
            break;
        }

        if (curr_token.type == eTokenType::Newline) {
            error_ = "Unexpected end of include path " + std::to_string(source_line_);
            break;
        }

        path.append(curr_token.raw_view);
    }

    while ((curr_token = GetNextToken()).type == eTokenType::Space)
        ; // skip space tokens

    if (curr_token.type != eTokenType::Newline && curr_token.type != eTokenType::End) {
        error_ = "Unexpected token " + std::to_string(source_line_);
        return false;
    }

    streams_.push_back(config_.include_callback(path.c_str(), is_system_path));
    if (!streams_.back() || !streams_.back()->good()) {
        error_ = "Failed to include '" + path + "'";
        return false;
    }
    return true;
}

bool glslx::Preprocessor::ProcessIf() {
    token_t curr_token = GetNextToken();
    if (curr_token.type == eTokenType::Space) {
        curr_token = GetNextToken();
    }

    std::vector<token_t> expression_tokens;
    while (curr_token.type != eTokenType::Newline) {
        if (curr_token.type != eTokenType::Space) {
            expression_tokens.push_back(std::move(curr_token));
        }
        curr_token = GetNextToken();
    }

    if (curr_token.type != eTokenType::Space && curr_token.type != eTokenType::Newline) {
        error_ = "Unexpected token at " + std::to_string(source_line_);
        return false;
    }

    if_blocks_.emplace_back();
    if_blocks_.back().should_be_skipped = !EvaluateExpression(expression_tokens);
    if (!if_blocks_.back().should_be_skipped) {
        if_blocks_.back().has_if_been_entered = true;
    }

    return true;
}

bool glslx::Preprocessor::ProcessIfdef() {
    token_t curr_token = GetNextToken();
    if (!expect(eTokenType::Space, curr_token.type)) {
        return false;
    }

    curr_token = GetNextToken();
    if (!expect(eTokenType::Identifier, curr_token.type)) {
        return false;
    }

    const std::string macro_identifier = curr_token.raw_view;

    curr_token = GetNextToken();
    if (curr_token.type != eTokenType::Space && curr_token.type != eTokenType::Newline) {
        error_ = "Unexpected token at " + std::to_string(source_line_);
        return false;
    }

    if_blocks_.emplace_back();
    if_blocks_.back().should_be_skipped =
        std::find_if(cbegin(macros_), cend(macros_), [&macro_identifier](const macro_desc_t &macro) {
            return macro.name == macro_identifier;
        }) == cend(macros_);
    if (!if_blocks_.back().should_be_skipped) {
        if_blocks_.back().has_if_been_entered = true;
    }

    return true;
}

bool glslx::Preprocessor::ProcessIfndef() {
    token_t curr_token = GetNextToken();
    if (!expect(eTokenType::Space, curr_token.type)) {
        return false;
    }

    curr_token = GetNextToken();
    if (!expect(eTokenType::Identifier, curr_token.type)) {
        return false;
    }

    const std::string macro_identifier = curr_token.raw_view;

    curr_token = GetNextToken();
    if (curr_token.type != eTokenType::Space && curr_token.type != eTokenType::Newline) {
        error_ = "Unexpected token at " + std::to_string(source_line_);
        return false;
    }

    if_blocks_.emplace_back();
    if_blocks_.back().should_be_skipped =
        std::find_if(cbegin(macros_), cend(macros_), [&macro_identifier](const macro_desc_t &macro) {
            return macro.name == macro_identifier;
        }) != cend(macros_);
    if (!if_blocks_.back().should_be_skipped) {
        if_blocks_.back().has_if_been_entered = true;
    }

    return true;
}

bool glslx::Preprocessor::ProcessElse() {
    if_block_t &block = if_blocks_.back();
    if (block.has_else_been_found) {
        error_ = "Another else block found " + std::to_string(source_line_);
        return false;
    }

    block.should_be_skipped = block.has_if_been_entered || !block.should_be_skipped;
    block.has_else_been_found = true;

    return true;
}

bool glslx::Preprocessor::ProcessElif() {
    if (if_blocks_.back().has_else_been_found) {
        error_ = "#elif after #else found " + std::to_string(source_line_);
        return false;
    }

    token_t curr_token = GetNextToken();
    if (curr_token.type == eTokenType::Space) {
        curr_token = GetNextToken();
    }

    std::vector<token_t> expression_tokens;
    while (curr_token.type != eTokenType::Newline) {
        if (curr_token.type != eTokenType::Space) {
            expression_tokens.push_back(std::move(curr_token));
        }
        curr_token = GetNextToken();
    }

    if (!expect(eTokenType::Newline, curr_token.type)) {
        return false;
    }

    if_blocks_.back().should_be_skipped =
        if_blocks_.back().has_if_been_entered || EvaluateExpression(expression_tokens) == 0;
    if (!if_blocks_.back().should_be_skipped) {
        if_blocks_.back().has_if_been_entered = true;
    }

    return true;
}

std::vector<glslx::Preprocessor::token_t>
glslx::Preprocessor::ExpandMacroDefinition(const macro_desc_t &macro, const token_t &token,
                                           const std::function<token_t()> &get_next_token) {
    if (macro.arg_names.empty()) {
        if (macro.name == "__LINE__") {
            return {{eTokenType::Blob, std::to_string(source_line_)}};
        }
        return macro.value;
    }

    context_stack_.push_back(macro.name);

    token_t curr_token;
    while ((curr_token = get_next_token()).type == eTokenType::Space)
        ; // skip space tokens
    if (!expect(eTokenType::Bracket_Begin, curr_token.type)) {
        return {};
    }

    std::vector<std::vector<token_t>> processing_tokens;

    int curr_nesting = 0;
    while (true) {
        std::vector<token_t> curr_arg_tokens;

        while ((curr_token = get_next_token()).type == eTokenType::Space)
            ; // skip space tokens

        while ((curr_token.type != eTokenType::Comma && curr_token.type != eTokenType::Newline &&
                curr_token.type != eTokenType::Bracket_End && curr_token.type != eTokenType::End) ||
               curr_nesting) {
            if (curr_token.type == eTokenType::Bracket_Begin) {
                ++curr_nesting;
            } else if (curr_token.type == eTokenType::Bracket_End) {
                --curr_nesting;
            }

            curr_arg_tokens.push_back(curr_token);
            curr_token = get_next_token();
        }

        if (curr_token.type != eTokenType::Comma && curr_token.type != eTokenType::Bracket_End) {
            if (!expect(eTokenType::Newline, curr_token.type)) {
                return {};
            }
            curr_token = get_next_token();
        }

        if (!curr_arg_tokens.empty()) {
            processing_tokens.push_back(std::move(curr_arg_tokens));
        }

        if (curr_token.type == eTokenType::Bracket_End) {
            break;
        }
    }

    if (processing_tokens.size() != macro.arg_names.size()) {
        error_ = "Inconsistent macro arity " + std::to_string(source_line_);
        return {};
    }

    std::vector<token_t> replacement_list = macro.value;

    for (int arg_index = 0; arg_index < int(processing_tokens.size()); ++arg_index) {
        const std::string &arg_name = macro.arg_names[arg_index];

        for (auto it = begin(replacement_list); it != end(replacement_list); ) {
            if (it->type == eTokenType::Identifier && it->raw_view == arg_name) {
                it = replacement_list.erase(it);
                it = replacement_list.insert(it, begin(processing_tokens[arg_index]), end(processing_tokens[arg_index]));
                it += processing_tokens[arg_index].size();
            } else {
                ++it;
            }
        }
    }

    replacement_list.push_back({eTokenType::Reject_Macro, macro.name});
    return replacement_list;
}

int glslx::Preprocessor::EvaluateExpression(Span<const token_t> _tokens) {
    std::vector<token_t> tokens(_tokens.begin(), _tokens.end());
    tokens.push_back({eTokenType::End});

    auto eval_prim = [this, &tokens]() {
        while (tokens.front().type == eTokenType::Space) {
            tokens.erase(tokens.cbegin());
        }

        token_t curr_token = tokens.front();

        switch (curr_token.type) {
        case eTokenType::Identifier: {
            token_t identifier_token;
            if (curr_token.raw_view == "defined") {
                do {
                    tokens.erase(tokens.cbegin());
                } while (tokens.front().type == eTokenType::Space);

                if (!expect(eTokenType::Bracket_Begin, tokens.front().type)) {
                    return 0;
                }

                do {
                    tokens.erase(tokens.cbegin());
                } while (tokens.front().type == eTokenType::Space);

                if (!expect(eTokenType::Identifier, tokens.front().type)) {
                    return 0;
                }

                identifier_token = tokens.front();

                do {
                    tokens.erase(tokens.cbegin());
                } while (tokens.front().type == eTokenType::Space);

                if (!expect(eTokenType::Bracket_End, tokens.front().type)) {
                    return 0;
                }

                do {
                    tokens.erase(tokens.cbegin());
                } while (tokens.front().type == eTokenType::Space);

                return int(std::find_if(cbegin(macros_), cend(macros_), [&identifier_token](auto &&item) {
                               return item.name == identifier_token.raw_view;
                           }) != cend(macros_));
            } else {
                tokens.erase(tokens.cbegin());
                identifier_token = curr_token;
            }

            auto it = std::find_if(cbegin(macros_), cend(macros_),
                                   [&identifier_token](auto &&item) { return item.name == identifier_token.raw_view; });

            if (it == cend(macros_)) {
                return atoi(identifier_token.raw_view.c_str());
            } else {
                if (it->arg_names.empty()) {
                    return EvaluateExpression(it->value); // simple macro replacement
                }

                auto curr_token_it = tokens.cbegin();
                return EvaluateExpression(
                    ExpandMacroDefinition(*it, identifier_token, [&curr_token_it] { return *curr_token_it++; }));
            }

            return 0;
        }

        case eTokenType::Number:
            tokens.erase(tokens.cbegin());
            return std::stoi(curr_token.raw_view);

        case eTokenType::Bracket_Begin:
            tokens.erase(tokens.cbegin());
            return EvaluateExpression(tokens);

        default:
            break;
        }

        return 0;
    };

    auto eval_unary = [&tokens, &eval_prim]() {
        while (tokens.front().type == eTokenType::Space) {
            tokens.erase(tokens.cbegin());
        }

        bool result_apply = false;
        token_t curr_token;
        while ((curr_token = tokens.front()).type == eTokenType::Not || curr_token.type == eTokenType::Not) {
            switch (curr_token.type) {
            case eTokenType::Minus:
                // TODO fix this
                break;
            case eTokenType::Not:
                tokens.erase(tokens.cbegin());
                result_apply = !result_apply;
                break;
            default:
                break;
            }
        }

        // even number of NOTs: false ^ false = false, false ^ true = true
        // odd number of NOTs: true ^ false = true (!false), true ^ true = false (!true)
        return static_cast<int>(result_apply) ^ eval_prim();
    };

    auto eval_muldiv = [&tokens, &eval_unary]() {
        int result = eval_unary();
        int second_operand = 0;

        token_t curr_token;
        while ((curr_token = tokens.front()).type == eTokenType::Star || curr_token.type == eTokenType::Slash) {
            switch (curr_token.type) {
            case eTokenType::Star:
                tokens.erase(tokens.cbegin());
                result = result * eval_unary();
                break;
            case eTokenType::Slash:
                tokens.erase(tokens.cbegin());

                second_operand = eval_unary();
                result = second_operand ? (result / second_operand) : 0;
                break;
            default:
                break;
            }
        }

        return result;
    };

    auto eval_addsub = [&tokens, &eval_muldiv]() {
        int result = eval_muldiv();

        token_t curr_token;
        while ((curr_token = tokens.front()).type == eTokenType::Plus || curr_token.type == eTokenType::Minus) {
            switch (curr_token.type) {
            case eTokenType::Plus:
                tokens.erase(tokens.cbegin());
                result = result + eval_muldiv();
                break;
            case eTokenType::Minus:
                tokens.erase(tokens.cbegin());
                result = result - eval_muldiv();
                break;
            default:
                break;
            }
        }

        return result;
    };

    auto eval_cmp = [&tokens, &eval_addsub]() {
        int result = eval_addsub();

        token_t curr_token;
        while ((curr_token = tokens.front()).type == eTokenType::Less || curr_token.type == eTokenType::Greater ||
               curr_token.type == eTokenType::LessEqual || curr_token.type == eTokenType::GreaterEqual) {
            switch (curr_token.type) {
            case eTokenType::Less:
                tokens.erase(tokens.cbegin());
                result = result < eval_addsub();
                break;
            case eTokenType::Greater:
                tokens.erase(tokens.cbegin());
                result = result > eval_addsub();
                break;
            case eTokenType::LessEqual:
                tokens.erase(tokens.cbegin());
                result = result <= eval_addsub();
                break;
            case eTokenType::GreaterEqual:
                tokens.erase(tokens.cbegin());
                result = result >= eval_addsub();
                break;
            default:
                break;
            }
        }

        return result;
    };

    auto eval_eq = [&tokens, &eval_cmp]() {
        int result = eval_cmp();

        token_t curr_token;
        while ((curr_token = tokens.front()).type == eTokenType::Equal || curr_token.type == eTokenType::NotEqual) {
            switch (curr_token.type) {
            case eTokenType::Equal:
                tokens.erase(tokens.cbegin());
                result = result == eval_cmp();
                break;
            case eTokenType::NotEqual:
                tokens.erase(tokens.cbegin());
                result = result != eval_cmp();
                break;
            default:
                break;
            }
        }

        return result;
    };

    auto eval_and = [&tokens, &eval_eq]() {
        int result = eval_eq();

        while (tokens.front().type == eTokenType::Space) {
            tokens.erase(tokens.cbegin());
        }

        while (tokens.front().type == eTokenType::And) {
            tokens.erase(tokens.cbegin());
            result &= eval_eq();
        }

        return result;
    };

    auto eval_or = [&tokens, &eval_and]() {
        int result = eval_and();

        while (tokens.front().type == eTokenType::Or) {
            tokens.erase(tokens.cbegin());
            result |= eval_and();
        }

        return result;
    };

    return eval_or();
}