#pragma once

#include <functional>
#include <string_view>

#include <Ren/HashMap32.h>
#include <Ren/SmallVector.h>
#include <Ren/Span.h>

namespace Eng {
class Cmdline {
  public:
    Cmdline();

    enum class eArgType { Number, String };

    struct ArgData {
        eArgType type;
        std::string_view str;
        double val;
    };
    static const int MaxHistoryCount = 8;

    using CommandHandler = std::function<bool(Ren::Span<const ArgData> args)>;

    void RegisterCommand(std::string_view cmd, const CommandHandler &handler);
    bool Execute(std::string_view str);

    int NextHint(std::string_view str, int i, Ren::String &out_str) const;

  private:
    Ren::HashMap32<Ren::String, CommandHandler> cmd_handlers_;

    bool Parse(std::string_view str, Ren::SmallVectorImpl<ArgData> &out_args);
};
} // namespace Eng