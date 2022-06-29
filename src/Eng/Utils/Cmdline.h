#pragma once

#include <functional>

#include <Ren/HashMap32.h>

class Cmdline {
public:
    Cmdline();

    enum class eArgType { ArgNumber, ArgString };

    struct ArgData {
        eArgType        type;
        Ren::StringPart str;
        double          val;
    };
    static const int MaxArgumentCount = 16;
    static const int MaxHistoryCount = 8;

    using CommandHandler = std::function<bool(int argc, ArgData *argv)>;

    void RegisterCommand(const char *cmd, const CommandHandler &handler);
    bool Execute(const char *str);

    int NextHint(const char *str, int i, Ren::String &out_str) const;

  private:
    Ren::HashMap32<Ren::String, CommandHandler> cmd_handlers_;

    bool Parse(const char *str, ArgData *out_argv, int &out_argc);
};