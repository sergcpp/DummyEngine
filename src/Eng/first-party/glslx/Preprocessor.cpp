#include "Preprocessor.h"

#include <fstream>
#include <sstream>

namespace glslx {
bool is_escape_sequence(const std::string_view str, size_t pos) {
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
    static const char Separators[] = ",()<>\"+-*/&|!=:;";
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

glslx::Preprocessor::Preprocessor(std::unique_ptr<std::istream> stream, preprocessor_config_t config)
    : alloc_(8, 128), config_(std::move(config)),
      directives_table_{{{"define", alloc_}, eTokenType::Define},
                        {{"ifdef", alloc_}, eTokenType::Ifdef},
                        {{"ifndef", alloc_}, eTokenType::Ifndef},
                        {{"if", alloc_}, eTokenType::If},
                        {{"else", alloc_}, eTokenType::Else},
                        {{"elif", alloc_}, eTokenType::Elif},
                        {{"undef", alloc_}, eTokenType::Undef},
                        {{"endif", alloc_}, eTokenType::Endif},
                        {{"include", alloc_}, eTokenType::Include},
                        {{"defined", alloc_}, eTokenType::Defined},
                        {{"extension", alloc_}, eTokenType::Extension},
                        {{"line", alloc_}, eTokenType::PassthroughDirective},
                        {{"version", alloc_}, eTokenType::PassthroughDirective},
                        {{"pragma", alloc_}, eTokenType::PassthroughDirective}},
      current_line_(alloc_), temp_str_(alloc_) {
    streams_.push_back(std::move(stream));
    macros_.emplace_back("__LINE__", alloc_);
    for (const macro_def_t &m : config_.default_macros) {
        macros_.emplace_back(m.name, alloc_);
        if (m.value != config_.empty_macro_value) {
            macros_.back().value.push_back({eTokenType::Number, std::to_string(m.value), alloc_});
        }
    }
}

glslx::Preprocessor::Preprocessor(std::string_view source, preprocessor_config_t config)
    : Preprocessor(std::make_unique<std::istringstream>(source.data()), std::move(config)) {}

glslx::Preprocessor::~Preprocessor() = default;

std::string glslx::Preprocessor::Process() {
    std::string output;

    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    while (curr_token.type != eTokenType::End) {
        switch (curr_token.type) {
        case eTokenType::Define:
            if (!CreateMacroDefinition()) {
                return {};
            }
            break;
        case eTokenType::Undef:
            GetNextToken(curr_token);
            if (!expect(eTokenType::Space, curr_token.type)) {
                return {};
            }

            GetNextToken(curr_token);
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
            // const auto macro_it =
            //     std::find_if(begin(macros_), end(macros_),
            //                  [&curr_token](const macro_desc_t &macro) { return
            //                  macro.name == curr_token.raw_view; });
            // const auto ctx_it =
            //     std::find_if(cbegin(context_stack_), cend(context_stack_),
            //                  [&curr_token](const std::string &name) { return name ==
            //                  curr_token.raw_view; });

            const macro_desc_t *macro = nullptr;
            for (const macro_desc_t &d : macros_) {
                if (d.name == curr_token.raw_view) {
                    macro = &d;
                }
            }

            if (macro != nullptr /*&& ctx_it == cend(context_stack_)*/) {
                const global_vector<token_t> expanded = ExpandMacroDefinition(
                    *macro, curr_token, std::bind(&Preprocessor::GetNextToken, this, std::placeholders::_1));
                if (expanded.empty() && !error_.empty()) {
                    return {};
                }
                tokens_queue_.insert(tokens_queue_.begin(), expanded.begin(), expanded.end());
            } else {
                if (!ShouldTokenBeSkipped()) {
                    output.append(curr_token.raw_view);
                }
            }
        } break;
        case eTokenType::Reject_Macro:
            context_stack_.erase(
                std::remove_if(begin(context_stack_), end(context_stack_),
                               [&curr_token](const local_string &name) { return name == curr_token.raw_view; }),
                end(context_stack_));
            break;
        case eTokenType::Concat_Op:
            while (output.back() == ' ') {
                output.pop_back();
            }

            GetNextToken(curr_token);
            while (curr_token.type == eTokenType::Space) {
                GetNextToken(curr_token); // skip space tokens
            }

            if (!ShouldTokenBeSkipped()) {
                output.append(curr_token.raw_view);
            }
            break;
        case eTokenType::Stringize_Op:
            GetNextToken(curr_token);
            output.append(curr_token.raw_view);
            break;
        case eTokenType::PassthroughDirective:
            if (!ShouldTokenBeSkipped()) {
                output.append("#");
                output.append(curr_token.raw_view);
            }
            break;
        case eTokenType::Extension:
            if (!ShouldTokenBeSkipped()) {
                output.append("#");
                output.append(curr_token.raw_view);
                if (!ProcessExtension(output)) {
                    return {};
                }
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
        GetNextToken(curr_token);
        if (curr_token.type == eTokenType::End && !streams_.empty()) {
            streams_.pop_back();
            GetNextToken(curr_token);
        }
    }
    return output;
}

void glslx::Preprocessor::ReadLine(local_string &out_line) {
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

void glslx::Preprocessor::RequestSourceLine(local_string &out_line) {
    ReadLine(out_line);
    size_t pos = 0;
    while ((pos = out_line.find_first_of('\\')) != std::string::npos &&
           (pos >= out_line.length() || std::isspace(out_line[pos + 1])) && !is_escape_sequence(out_line, pos)) {
        local_string next_line(alloc_);
        ReadLine(next_line);
        if (!next_line.empty()) {
            out_line.replace(pos ? (pos - 1) : 0, std::string::npos, next_line);
        } else {
            out_line.erase(begin(out_line) + pos, end(out_line));
        }
    }
}

void glslx::Preprocessor::GetNextToken(token_t &out_tok) {
    if (!tokens_queue_.empty()) {
        out_tok = std::move(tokens_queue_.front());
        tokens_queue_.pop_front();
        return;
    }

    if (current_line_.empty()) {
        RequestSourceLine(current_line_);
        if (current_line_.empty()) {
            out_tok.set(eTokenType::End);
            return;
        }
    }

    ScanTokens(out_tok, current_line_);
}

void glslx::Preprocessor::ScanTokens(token_t &out_tok, local_string &inout_line) {
    out_tok.raw_view.clear();
    temp_str_.clear();

    char ch = '\0';
    while (!inout_line.empty()) {
        ch = inout_line.front();
        if (ch == '/') {
            local_string &comment_str = out_tok.raw_view;
            if (inout_line.length() > 1 && inout_line[1] == '/') { // single line comment
                comment_str = ExtractSingleLineComment(inout_line);
            } else if (inout_line.length() > 1 && inout_line[1] == '*') { // multi-line comment
                comment_str = ExtractMultiLineComment(inout_line);
            }

            if (!comment_str.empty()) {
                inout_line.erase(0, comment_str.length());
                curr_pos_ += comment_str.length();

                out_tok.type = eTokenType::Comment;
                out_tok.line = source_line_;
                out_tok.pos = curr_pos_;
                return;
            }
        }

        if (ch == '\n' || ch == '\r') {
            if (!temp_str_.empty()) {
                out_tok = {eTokenType::Blob, std::move(temp_str_), source_line_};
                return;
            }

            out_tok.raw_view.clear();
            out_tok.raw_view.push_back(inout_line.front());

            const char next = inout_line[1];
            if (ch == '\r' && next == '\n') {
                out_tok.raw_view.push_back(next);
            }

            inout_line.erase(0, out_tok.raw_view.length());
            curr_pos_ += out_tok.raw_view.length();

            out_tok.type = eTokenType::Newline;
            out_tok.line = source_line_;
            out_tok.pos = curr_pos_;

            return;
        }

        if (isspace(ch)) {
            if (!temp_str_.empty()) {
                out_tok = {eTokenType::Blob, std::move(temp_str_), source_line_};
                return;
            }

            out_tok.type = eTokenType::Space;
            out_tok.raw_view.clear();
            out_tok.raw_view.push_back(inout_line.front());
            out_tok.line = source_line_;
            out_tok.pos = ++curr_pos_;

            inout_line.erase(0, 1);
            return;
        }

        if (ch == '#') { // operator or directive
            if (!temp_str_.empty()) {
                out_tok = {eTokenType::Blob, std::move(temp_str_), source_line_};
                return;
            }

            do {
                inout_line.erase(0, 1);
                ++curr_pos_;
            } while (isspace(inout_line.front()));

            for (const auto &dir : directives_table_) {
                if (inout_line.rfind(dir.first, 0) == 0) {
                    inout_line.erase(0, dir.first.length());
                    curr_pos_ += dir.first.length();
                    out_tok.set(dir.second, dir.first, source_line_, curr_pos_);
                    return;
                }
            }

            if (!inout_line.empty()) { // operator
                const char next = inout_line.front();
                if (next == '#') { // concatenation operator
                    inout_line.erase(0, 1);
                    ++curr_pos_;
                    out_tok.set(eTokenType::Concat_Op, {}, source_line_, curr_pos_);
                    return;
                } else if (next != ' ') { // stringification operator
                    out_tok.set(eTokenType::Stringize_Op, {}, source_line_, curr_pos_);
                    return;
                } else {
                    out_tok.set(eTokenType::Blob, "#", source_line_, curr_pos_);
                    return;
                }
            }
        }

        if (isdigit(ch)) { // number
            if (!temp_str_.empty()) {
                out_tok = {eTokenType::Blob, std::move(temp_str_), source_line_};
                return;
            }

            local_string number(alloc_);

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
                    out_tok = {eTokenType::Number, std::move(number), source_line_, curr_pos_};
                    return;
                }
            }

            int i = 0;
            while ((i < inout_line.length()) && isdigit(ch = inout_line[i++])) {
                number.push_back(ch);
            }

            inout_line.erase(0, i - 1);
            curr_pos_ += i - 1;

            out_tok = {eTokenType::Number, std::move(number), source_line_, curr_pos_};
            return;
        }

        if (isalpha(ch) || ch == '_') { // idendifier
            if (!temp_str_.empty()) {
                out_tok = {eTokenType::Blob, std::move(temp_str_), source_line_};
                return;
            }

            out_tok.raw_view.clear();
            do {
                out_tok.raw_view.push_back(ch);
                inout_line.erase(0, 1);
                ++curr_pos_;
            } while (!inout_line.empty() && (isalnum(ch = inout_line.front()) || (ch == '_')));

            out_tok.type = eTokenType::Identifier;
            out_tok.line = source_line_;
            out_tok.pos = curr_pos_;
            return;
        }

        inout_line.erase(0, 1);

        if (is_separator(ch)) {
            if (!temp_str_.empty()) {
                token_t separator_token(alloc_);
                ScanSeparator(separator_token, ch, inout_line);
                if (separator_token.type != eTokenType::End) {
                    tokens_queue_.push_front(std::move(separator_token));
                }
                out_tok = {eTokenType::Blob, std::move(temp_str_), source_line_, curr_pos_};
                return;
            }
            ScanSeparator(out_tok, ch, inout_line);
            return;
        }

        temp_str_.push_back(ch);
    }

    if (!temp_str_.empty()) {
        out_tok = {eTokenType::Blob, std::move(temp_str_), source_line_, curr_pos_};
        return;
    }

    if (!streams_.empty()) {
        streams_.pop_back();
        GetNextToken(out_tok);
        return;
    }

    out_tok.set(eTokenType::End);
}

void glslx::Preprocessor::ScanSeparator(token_t &out_tok, const char ch, local_string &inout_line) {
    out_tok.raw_view.clear();

    switch (ch) {
    case ',':
        out_tok.set(eTokenType::Comma, ",", source_line_, curr_pos_);
        return;
    case '(':
        out_tok.set(eTokenType::Bracket_Begin, "(", source_line_, curr_pos_);
        return;
    case ')':
        out_tok.set(eTokenType::Bracket_End, ")", source_line_, curr_pos_);
        return;
    case '<':
        if (!inout_line.empty()) {
            const char next = inout_line.front();
            if (next == '<') {
                inout_line.erase(0, 1);
                ++curr_pos_;
                out_tok.set(eTokenType::LShift, "<<", source_line_, curr_pos_);
                return;
            } else if (next == '=') {
                inout_line.erase(0, 1);
                ++curr_pos_;
                out_tok.set(eTokenType::LessEqual, "<=", source_line_, curr_pos_);
                return;
            }
        }
        out_tok.set(eTokenType::Less, "<", source_line_, curr_pos_);
        return;
    case '>':
        if (!inout_line.empty()) {
            const char next = inout_line.front();
            if (next == '>') {
                inout_line.erase(0, 1);
                ++curr_pos_;
                out_tok.set(eTokenType::RShift, ">>", source_line_, curr_pos_);
                return;
            } else if (next == '=') {
                inout_line.erase(0, 1);
                ++curr_pos_;
                out_tok.set(eTokenType::GreaterEqual, ">=", source_line_, curr_pos_);
                return;
            }
        }
        out_tok.set(eTokenType::Greater, ">", source_line_, curr_pos_);
        return;
    case '\"':
        out_tok.set(eTokenType::Quotes, "\"", source_line_, curr_pos_);
        return;
    case '+':
        out_tok.set(eTokenType::Plus, "+", source_line_, curr_pos_);
        return;
    case '-':
        out_tok.set(eTokenType::Minus, "-", source_line_, curr_pos_);
        return;
    case '*':
        out_tok.set(eTokenType::Star, "*", source_line_, curr_pos_);
        return;
    case '/':
        out_tok.set(eTokenType::Slash, "/", source_line_, curr_pos_);
        return;
    case '&':
        if (!inout_line.empty() && inout_line.front() == '&') {
            inout_line.erase(0, 1);
            ++curr_pos_;
            out_tok.set(eTokenType::And, "&&", source_line_, curr_pos_);
            return;
        }
        out_tok.set(eTokenType::Ampersand, "&", source_line_, curr_pos_);
        return;
    case '|':
        if (!inout_line.empty() && inout_line.front() == '|') {
            inout_line.erase(0, 1);
            ++curr_pos_;
            out_tok.set(eTokenType::Or, "||", source_line_, curr_pos_);
            return;
        }
        out_tok.set(eTokenType::Vline, "|", source_line_, curr_pos_);
        return;
    case '!':
        if (!inout_line.empty() && inout_line.front() == '=') {
            inout_line.erase(0, 1);
            ++curr_pos_;
            out_tok.set(eTokenType::NotEqual, "!=", source_line_, curr_pos_);
            return;
        }
        out_tok.set(eTokenType::Not, "!", source_line_, curr_pos_);
        return;
    case '=':
        if (!inout_line.empty() && inout_line.front() == '=') {
            inout_line.erase(0, 1);
            ++curr_pos_;
            out_tok.set(eTokenType::Equal, "==", source_line_, curr_pos_);
            return;
        }
        out_tok.set(eTokenType::Blob, "=", source_line_, curr_pos_);
        return;
    case ':':
        out_tok.set(eTokenType::Colon, ":", source_line_, curr_pos_);
        return;
    case ';':
        out_tok.set(eTokenType::Semicolon, ";", source_line_, curr_pos_);
        return;
    }
    out_tok.set(eTokenType::End);
}

glslx::local_string glslx::Preprocessor::ExtractSingleLineComment(const local_string &line) {
    local_string ret = line;
    while (ret.back() == '\n') {
        ret.pop_back();
    }
    return ret;
}

glslx::local_string glslx::Preprocessor::ExtractMultiLineComment(local_string &line) {
    local_string ret(alloc_);

    local_string input = line;
    size_t pos;
    while ((pos = input.find("*/")) == std::string::npos && !input.empty()) {
        ret += input;
        RequestSourceLine(input);
        line += input;
    }
    ret.append(input, 0, pos + 2);

    return ret;
}

bool glslx::Preprocessor::CreateMacroDefinition() {
    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    if (!expect(eTokenType::Space, curr_token.type)) {
        return false;
    }

    GetNextToken(curr_token);
    if (!expect(eTokenType::Identifier, curr_token.type)) {
        return false;
    }

    macro_desc_t macro_desc(curr_token.raw_view, alloc_);

    auto extract_value = [this](global_vector<token_t> &value) {
        token_t curr_token(alloc_);
        GetNextToken(curr_token);
        while (curr_token.type == eTokenType::Space) {
            GetNextToken(curr_token); // skip space tokens
        }

        while (curr_token.type != eTokenType::Newline) {
            if (curr_token.type != eTokenType::Comment) {
                value.push_back(curr_token);
            }
            GetNextToken(curr_token);
        }

        if (value.empty() && config_.empty_macro_value != INT_MAX) {
            value.push_back({eTokenType::Number, std::to_string(config_.empty_macro_value), alloc_, source_line_});
        }

        if (!expect(eTokenType::Newline, curr_token.type)) {
            return false;
        }
        return true;
    };

    GetNextToken(curr_token);
    switch (curr_token.type) {
    case eTokenType::Space:
        if (!extract_value(macro_desc.value)) {
            return false;
        }
        break;
    case eTokenType::Newline:
    case eTokenType::End:
        if (config_.empty_macro_value != INT_MAX) {
            macro_desc.value.push_back(
                {eTokenType::Number, std::to_string(config_.empty_macro_value), alloc_, source_line_});
        }
        break;
    case eTokenType::Bracket_Begin: {
        while (true) {
            GetNextToken(curr_token);
            while (curr_token.type == eTokenType::Space) {
                GetNextToken(curr_token); // skip space tokens
            }

            if (!expect(eTokenType::Identifier, curr_token.type)) {
                return false;
            }
            macro_desc.arg_names.push_back(curr_token.raw_view);

            GetNextToken(curr_token);
            while (curr_token.type == eTokenType::Space) {
                GetNextToken(curr_token); // skip space tokens
            }
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
        error_ = std::string("Macro '") + macro_desc.name.c_str() + "' has already been defined " +
                 std::to_string(source_line_);
        return false;
    }

    macros_.push_back(macro_desc);

    return true;
}

bool glslx::Preprocessor::RemoveMacroDefinition(std::string_view macro_name) {
    if (ShouldTokenBeSkipped()) {
        return true;
    }

    auto it = std::find_if(cbegin(macros_), cend(macros_),
                           [&macro_name](const macro_desc_t &macro) { return macro.name == macro_name; });
    if (it == cend(macros_)) {
        return true;
    }

    macros_.erase(it);

    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    if (!expect(eTokenType::Newline, curr_token.type)) {
        return false;
    }

    return true;
}

bool glslx::Preprocessor::ProcessInclude() {
    if (ShouldTokenBeSkipped()) {
        return true;
    }

    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    while (curr_token.type == eTokenType::Space) {
        GetNextToken(curr_token); // skip space tokens
    }

    if (curr_token.type != eTokenType::Less && curr_token.type != eTokenType::Quotes) {
        while (curr_token.type == eTokenType::Newline) {
            GetNextToken(curr_token); // skip to the end of current line
        }

        error_ = "Invalid include directive " + std::to_string(source_line_);
        return false;
    }

    const bool is_system_path = (curr_token.type == eTokenType::Less);

    std::string path;

    while (true) {
        GetNextToken(curr_token);
        if (curr_token.type == eTokenType::Quotes || curr_token.type == eTokenType::Greater) {
            break;
        }

        if (curr_token.type == eTokenType::Newline) {
            error_ = "Unexpected end of include path " + std::to_string(source_line_);
            break;
        }

        path.append(curr_token.raw_view);
    }

    GetNextToken(curr_token);
    while (curr_token.type == eTokenType::Space) {
        GetNextToken(curr_token); // skip space tokens
    }

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

bool glslx::Preprocessor::ProcessExtension(std::string &output) {
    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    while (curr_token.type == eTokenType::Space) {
        output.append(curr_token.raw_view);
        GetNextToken(curr_token);
    }
    output.append(curr_token.raw_view);
    if (!expect(eTokenType::Identifier, curr_token.type)) {
        return false;
    }
    const local_string extension_name = curr_token.raw_view;
    GetNextToken(curr_token);
    while (curr_token.type == eTokenType::Space) {
        output.append(curr_token.raw_view);
        GetNextToken(curr_token);
    }
    output.append(curr_token.raw_view);
    if (!expect(eTokenType::Colon, curr_token.type)) {
        return false;
    }
    GetNextToken(curr_token);
    while (curr_token.type == eTokenType::Space) {
        output.append(curr_token.raw_view);
        GetNextToken(curr_token);
    }
    output.append(curr_token.raw_view);
    if (!expect(eTokenType::Identifier, curr_token.type)) {
        return false;
    }
    if (curr_token.raw_view != "disable") {
        macro_desc_t macro_desc(extension_name, alloc_);
        macro_desc.value.push_back({eTokenType::Number, "1", alloc_, source_line_});

        macros_.push_back(macro_desc);
    }
    return true;
}

bool glslx::Preprocessor::ProcessIf() {
    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    if (curr_token.type == eTokenType::Space) {
        GetNextToken(curr_token);
    }

    global_vector<token_t> expression_tokens;
    while (curr_token.type != eTokenType::Newline) {
        if (curr_token.type != eTokenType::Space) {
            expression_tokens.push_back(std::move(curr_token));
        }
        GetNextToken(curr_token);
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
    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    if (!expect(eTokenType::Space, curr_token.type)) {
        return false;
    }

    GetNextToken(curr_token);
    if (!expect(eTokenType::Identifier, curr_token.type)) {
        return false;
    }

    const local_string macro_identifier = curr_token.raw_view;

    GetNextToken(curr_token);
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
    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    if (!expect(eTokenType::Space, curr_token.type)) {
        return false;
    }

    GetNextToken(curr_token);
    if (!expect(eTokenType::Identifier, curr_token.type)) {
        return false;
    }

    const local_string macro_identifier = curr_token.raw_view;

    GetNextToken(curr_token);
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

    token_t curr_token(alloc_);
    GetNextToken(curr_token);
    if (curr_token.type == eTokenType::Space) {
        GetNextToken(curr_token);
    }

    global_vector<token_t> expression_tokens;
    while (curr_token.type != eTokenType::Newline) {
        if (curr_token.type != eTokenType::Space) {
            expression_tokens.push_back(std::move(curr_token));
        }
        GetNextToken(curr_token);
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

glslx::global_vector<glslx::Preprocessor::token_t>
glslx::Preprocessor::ExpandMacroDefinition(const macro_desc_t &macro, const token_t &token,
                                           const std::function<void(token_t &)> &get_next_token) {
    if (macro.arg_names.empty()) {
        if (macro.name == "__LINE__") {
            return {{eTokenType::Blob, std::to_string(source_line_), alloc_}};
        }
        return macro.value;
    }

    context_stack_.push_back(macro.name);

    token_t curr_token(alloc_);
    get_next_token(curr_token);
    while (curr_token.type == eTokenType::Space) {
        get_next_token(curr_token); // skip space tokens
    }
    if (!expect(eTokenType::Bracket_Begin, curr_token.type)) {
        return {};
    }

    global_vector<global_vector<token_t>> processing_tokens;

    int curr_nesting = 0;
    while (true) {
        global_vector<token_t> curr_arg_tokens;

        get_next_token(curr_token);
        while (curr_token.type == eTokenType::Space) {
            get_next_token(curr_token); // skip space tokens
        }

        while ((curr_token.type != eTokenType::Comma && curr_token.type != eTokenType::Newline &&
                curr_token.type != eTokenType::Bracket_End && curr_token.type != eTokenType::End) ||
               curr_nesting) {
            if (curr_token.type == eTokenType::Bracket_Begin) {
                ++curr_nesting;
            } else if (curr_token.type == eTokenType::Bracket_End) {
                --curr_nesting;
            }

            curr_arg_tokens.push_back(curr_token);
            get_next_token(curr_token);
        }

        if (curr_token.type != eTokenType::Comma && curr_token.type != eTokenType::Bracket_End) {
            if (!expect(eTokenType::Newline, curr_token.type)) {
                return {};
            }
            get_next_token(curr_token);
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

    global_vector<token_t> replacement_list = macro.value;

    for (int arg_index = 0; arg_index < int(processing_tokens.size()); ++arg_index) {
        const local_string &arg_name = macro.arg_names[arg_index];

        for (auto it = replacement_list.begin(); it != replacement_list.end();) {
            if (it->type == eTokenType::Identifier && it->raw_view == arg_name) {
                it = replacement_list.erase(it);
                it = replacement_list.insert(it, processing_tokens[arg_index].begin(),
                                             processing_tokens[arg_index].end());
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
    global_vector<token_t> tokens(_tokens.begin(), _tokens.end());
    tokens.push_back({eTokenType::End, alloc_});

    auto eval_prim = [this, &tokens]() {
        while (tokens.front().type == eTokenType::Space) {
            tokens.erase(tokens.begin());
        }

        token_t curr_token = tokens.front();

        switch (curr_token.type) {
        case eTokenType::Identifier: {
            token_t identifier_token(alloc_);
            if (curr_token.raw_view == "defined") {
                do {
                    tokens.erase(tokens.begin());
                } while (tokens.front().type == eTokenType::Space);

                if (!expect(eTokenType::Bracket_Begin, tokens.front().type)) {
                    return 0;
                }

                do {
                    tokens.erase(tokens.begin());
                } while (tokens.front().type == eTokenType::Space);

                if (!expect(eTokenType::Identifier, tokens.front().type)) {
                    return 0;
                }

                identifier_token = tokens.front();

                do {
                    tokens.erase(tokens.begin());
                } while (tokens.front().type == eTokenType::Space);

                if (!expect(eTokenType::Bracket_End, tokens.front().type)) {
                    return 0;
                }

                do {
                    tokens.erase(tokens.begin());
                } while (tokens.front().type == eTokenType::Space);

                return int(std::find_if(cbegin(macros_), cend(macros_), [&identifier_token](auto &&item) {
                               return item.name == identifier_token.raw_view;
                           }) != cend(macros_));
            } else {
                tokens.erase(tokens.begin());
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
                return EvaluateExpression(ExpandMacroDefinition(
                    *it, identifier_token, [&curr_token_it](token_t &out_tok) { out_tok = *curr_token_it++; }));
            }

            return 0;
        }

        case eTokenType::Number:
            tokens.erase(tokens.begin());
            return std::stoi(curr_token.raw_view.c_str());

        case eTokenType::Bracket_Begin:
            tokens.erase(tokens.begin());
            return EvaluateExpression(tokens);

        default:
            break;
        }

        return 0;
    };

    auto eval_unary = [this, &tokens, &eval_prim]() {
        while (tokens.front().type == eTokenType::Space) {
            tokens.erase(tokens.begin());
        }

        bool result_apply = false;
        token_t curr_token(alloc_);
        while ((curr_token = tokens.front()).type == eTokenType::Not || curr_token.type == eTokenType::Not) {
            switch (curr_token.type) {
            case eTokenType::Minus:
                // TODO fix this
                break;
            case eTokenType::Not:
                tokens.erase(tokens.begin());
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

    auto eval_muldiv = [this, &tokens, &eval_unary]() {
        int result = eval_unary();
        int second_operand = 0;

        token_t curr_token(alloc_);
        while ((curr_token = tokens.front()).type == eTokenType::Star || curr_token.type == eTokenType::Slash) {
            switch (curr_token.type) {
            case eTokenType::Star:
                tokens.erase(tokens.begin());
                result = result * eval_unary();
                break;
            case eTokenType::Slash:
                tokens.erase(tokens.begin());

                second_operand = eval_unary();
                result = second_operand ? (result / second_operand) : 0;
                break;
            default:
                break;
            }
        }

        return result;
    };

    auto eval_addsub = [this, &tokens, &eval_muldiv]() {
        int result = eval_muldiv();

        token_t curr_token(alloc_);
        while ((curr_token = tokens.front()).type == eTokenType::Plus || curr_token.type == eTokenType::Minus) {
            switch (curr_token.type) {
            case eTokenType::Plus:
                tokens.erase(tokens.begin());
                result = result + eval_muldiv();
                break;
            case eTokenType::Minus:
                tokens.erase(tokens.begin());
                result = result - eval_muldiv();
                break;
            default:
                break;
            }
        }

        return result;
    };

    auto eval_cmp = [this, &tokens, &eval_addsub]() {
        int result = eval_addsub();

        token_t curr_token(alloc_);
        while ((curr_token = tokens.front()).type == eTokenType::Less || curr_token.type == eTokenType::Greater ||
               curr_token.type == eTokenType::LessEqual || curr_token.type == eTokenType::GreaterEqual) {
            switch (curr_token.type) {
            case eTokenType::Less:
                tokens.erase(tokens.begin());
                result = result < eval_addsub();
                break;
            case eTokenType::Greater:
                tokens.erase(tokens.begin());
                result = result > eval_addsub();
                break;
            case eTokenType::LessEqual:
                tokens.erase(tokens.begin());
                result = result <= eval_addsub();
                break;
            case eTokenType::GreaterEqual:
                tokens.erase(tokens.begin());
                result = result >= eval_addsub();
                break;
            default:
                break;
            }
        }

        return result;
    };

    auto eval_eq = [this, &tokens, &eval_cmp]() {
        int result = eval_cmp();

        token_t curr_token(alloc_);
        while ((curr_token = tokens.front()).type == eTokenType::Equal || curr_token.type == eTokenType::NotEqual) {
            switch (curr_token.type) {
            case eTokenType::Equal:
                tokens.erase(tokens.begin());
                result = result == eval_cmp();
                break;
            case eTokenType::NotEqual:
                tokens.erase(tokens.begin());
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
            tokens.erase(tokens.begin());
        }

        while (tokens.front().type == eTokenType::And) {
            tokens.erase(tokens.begin());
            result &= eval_eq();
        }

        return result;
    };

    auto eval_or = [&tokens, &eval_and]() {
        int result = eval_and();

        while (tokens.front().type == eTokenType::Or) {
            tokens.erase(tokens.begin());
            result |= eval_and();
        }

        return result;
    };

    return eval_or();
}