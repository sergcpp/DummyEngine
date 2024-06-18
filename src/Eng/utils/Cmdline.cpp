#include "Cmdline.h"

#include <cctype>

Eng::Cmdline::Cmdline() {}

void Eng::Cmdline::RegisterCommand(const char *cmd, const CommandHandler &handler) {
    cmd_handlers_[Ren::String{cmd}] = handler;
}

bool Eng::Cmdline::Execute(const char *str) {
    Ren::SmallVector<ArgData, 16> args;
    if (Parse(str, args)) {
        const CommandHandler *handler = cmd_handlers_.Find(args[0].str);
        if (handler) {
            return (*handler)(args);
        }
    }

    return false;
}

int Eng::Cmdline::NextHint(const char *str, const int i, Ren::String &out_str) const {
    auto it = (i == -1) ? cmd_handlers_.cbegin() : ++cmd_handlers_.citer_at(i);
    for (; it != cmd_handlers_.cend(); ++it) {
        if (it->key.StartsWith(str)) {
            out_str = it->key;
            return it.index();
        }
    }
    return -1;
}

bool Eng::Cmdline::Parse(const char *str, Ren::SmallVectorImpl<ArgData> &out_args) {
    const char *s = str;
    // skip white space
    while (std::isspace(*s))
        s++;
    // check if command is valid
    if (!std::isalpha(*s)) {
        return false;
    }

    while (*s) {
        while (std::isspace(*s))
            s++;
        const char *tok_start = s;
        while (*s && !std::isspace(*s))
            s++;

        ArgData &arg = out_args.emplace_back();
        arg.str = std::string_view(tok_start, s - tok_start);
    }

    for (int i = 0; i < int(out_args.size()); i++) {
        ArgData &arg = out_args[i];
        if (arg.str.empty()) {
            continue;
        }

        if (arg.str[0] == '"') {
            if (arg.str[arg.str.length() - 1] != '"') {
                return false;
            }
            arg.str = arg.str.substr(1, arg.str.length() - 2);
            arg.type = eArgType::ArgString;
        } else if (std::isalpha(arg.str[0])) {
            arg.type = eArgType::ArgString;
        } else {
            arg.type = eArgType::ArgNumber;

            // TODO: refactor this
            char temp_buf[128];
            memcpy(temp_buf, arg.str.data(), arg.str.length());
            temp_buf[arg.str.length()] = '\0';

            arg.val = strtod(temp_buf, nullptr);
        }
    }

    return !out_args.empty() && out_args[0].type == eArgType::ArgString;
}