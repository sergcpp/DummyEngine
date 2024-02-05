#include "Cmdline.h"

#include <cctype>

Eng::Cmdline::Cmdline() {}

void Eng::Cmdline::RegisterCommand(const char *cmd, const CommandHandler &handler) {
    cmd_handlers_[Ren::String{cmd}] = handler;
}

bool Eng::Cmdline::Execute(const char *str) {
    ArgData argv[MaxArgumentCount];
    int argc = 0;

    if (Parse(str, argv, argc)) {
        const CommandHandler *handler = cmd_handlers_.Find(argv[0].str);
        if (handler) {
            return (*handler)(argc, argv);
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

bool Eng::Cmdline::Parse(const char *str, ArgData *out_argv, int &out_argc) {
    const char *s = str;
    // skip white space
    while (std::isspace(*s))
        s++;
    // check if command is valid
    if (!std::isalpha(*s)) {
        return false;
    }

    out_argc = 0;

    while (*s) {
        while (std::isspace(*s))
            s++;
        const char *tok_start = s;
        while (*s && !std::isspace(*s))
            s++;

        out_argv[out_argc].str = std::string_view(tok_start, s - tok_start);
        out_argc++;
    }

    for (int i = 0; i < out_argc; i++) {
        ArgData &arg = out_argv[i];

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

    return out_argc && out_argv[0].type == eArgType::ArgString;
}