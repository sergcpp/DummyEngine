#pragma once

#include <functional>

#include <Ren/HashMap32.h>
#include <Ren/SmallVector.h>
#include <Ren/Span.h>

namespace Eng {
class Cmdline {
  public:
    Cmdline();

    enum class eArgType { ArgNumber, ArgString };

    struct ArgData {
        eArgType type;
        std::string_view str;
        double val;
    };
    static const int MaxHistoryCount = 8;

    using CommandHandler = std::function<bool(Ren::Span<const ArgData> args)>;

    void RegisterCommand(const char *cmd, const CommandHandler &handler);
    bool Execute(const char *str);

    int NextHint(const char *str, int i, Ren::String &out_str) const;

  private:
    Ren::HashMap32<Ren::String, CommandHandler> cmd_handlers_;

    bool Parse(const char *str, Ren::SmallVectorImpl<ArgData> &out_args);
};
} // namespace Eng