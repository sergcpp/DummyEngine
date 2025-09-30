#pragma once

#include <functional>
#include <string_view>

#include <Gui/Image9Patch.h>
#include <Ren/HashMap32.h>
#include <Ren/SmallVector.h>
#include <Ren/Span.h>

namespace Eng {
class CmdlineUI final : public Gui::BaseElement {
  public:
    CmdlineUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
              Gui::BaseElement *parent);

    enum class eArgType { Number, String };

    struct ArgData {
        eArgType type;
        std::string_view str;
        double num;
    };
    static const int MaxHistoryLength = 64;

    using CommandHandler = std::function<bool(Ren::Span<const ArgData> args)>;

    void RegisterCommand(std::string_view cmd, const CommandHandler &handler);
    bool ExecuteCommand(std::string_view str);
    void Serve();

    int NextHint(std::string_view str, int i, Ren::String &out_str) const;

    bool HandleInput(const Gui::input_event_t &ev, const std::vector<bool> &keys_state) override;

    void Draw(Gui::Renderer *r) override;

    bool enabled = false;
    uint64_t cursor_blink_us = 0;

  private:
    Gui::Image9Patch back_;
    const Gui::BitmapFont &font_;

    Ren::HashMap32<Ren::String, CommandHandler> cmd_handlers_;

    std::string command_to_execute_;
    std::vector<std::string> history_;
    int history_index_ = -1;

    bool Parse(std::string_view str, Ren::SmallVectorImpl<ArgData> &out_args);
};
} // namespace Eng