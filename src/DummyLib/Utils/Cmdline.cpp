#include "Cmdline.h"

#include <cctype>

Cmdline::Cmdline() {
}

void Cmdline::RegisterCommand(const char *cmd, const CommandHandler &handler) {
    cmd_handlers_[Ren::String{ cmd }] = handler;
}

bool Cmdline::Execute(const char *str) {
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

bool Cmdline::Parse(const char *str, ArgData *out_argv, int &out_argc) {
    const char *s = str;
    // skip white space
    while (std::isspace(*s)) s++;
    // check if command is valid
    if (!std::isalpha(*s)) {
        return false;
    }

    out_argc = 0;

    while (*s) {
        while (std::isspace(*s)) s++;
        const char *tok_start = s;
        while (*s && !std::isspace(*s)) s++;

        out_argv[out_argc].str.str = tok_start;
        out_argv[out_argc].str.len = s - tok_start;
        out_argc++;
    }

    for (int i = 0; i < out_argc; i++) {
        ArgData &arg = out_argv[i];

        if (arg.str.str[0] == '"') {
            if (arg.str.str[arg.str.len - 1] != '"') {
                return false;
            }

            arg.str.str++;
            arg.str.len -= 2;

            arg.type = ArgString;
        } else if (std::isalpha(arg.str.str[0])) {
            arg.type = ArgString;
        } else {
            arg.type = ArgNumber;

            char temp_buf[128];
            memcpy(temp_buf, arg.str.str, arg.str.len);
            temp_buf[arg.str.len] = '\0';

            arg.val = strtod(temp_buf, nullptr);
        }
    }

    return out_argc && out_argv[0].type == ArgString;
}