#include "CmdlineUI.h"

#include <cctype>

#include <Gui/BitmapFont.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>

#undef DrawText
#include "../input/InputManager.h"

Eng::CmdlineUI::CmdlineUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
                          Gui::BaseElement *parent)
    : Gui::BaseElement(pos, size, parent), back_(ctx, "assets_pc/textures/internal/back.dds", Gui::Vec2f{1.5f}, 1.0f,
                                                 Gui::Vec2f{-1.0f, 0.0f}, Gui::Vec2f{2.0f, 1.0f}, this),
      font_(font) {
    history_.emplace_back();
}

void Eng::CmdlineUI::RegisterCommand(std::string_view cmd, const CommandHandler &handler) {
    cmd_handlers_[Ren::String{cmd}] = handler;
}

bool Eng::CmdlineUI::ExecuteCommand(std::string_view str) {
    Ren::SmallVector<ArgData, 16> args;
    if (Parse(str, args)) {
        const CommandHandler *handler = cmd_handlers_.Find(args[0].str);
        if (handler) {
            return (*handler)(args);
        }
    }

    return false;
}

void Eng::CmdlineUI::Serve() {
    if (!command_to_execute_.empty()) {
        ExecuteCommand(command_to_execute_);
        command_to_execute_.clear();
    }
}

int Eng::CmdlineUI::NextHint(std::string_view str, const int i, Ren::String &out_str) const {
    auto it = (i == -1) ? cmd_handlers_.cbegin() : ++cmd_handlers_.citer_at(i);
    for (; it != cmd_handlers_.cend(); ++it) {
        if (it->key.StartsWith(str)) {
            out_str = it->key;
            return it.index();
        }
    }
    return -1;
}

bool Eng::CmdlineUI::Parse(std::string_view str, Ren::SmallVectorImpl<ArgData> &out_args) {
    const char *s = str.data();
    // skip white space
    while (std::isspace(*s)) {
        s++;
    }
    // check if command is valid
    if (!std::isalpha(*s)) {
        return false;
    }

    while (*s) {
        while (std::isspace(*s)) {
            s++;
        }
        const char *tok_start = s;
        while (*s && !std::isspace(*s)) {
            s++;
        }

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
            arg.type = eArgType::String;
        } else if (std::isalpha(arg.str[0])) {
            arg.type = eArgType::String;
        } else {
            arg.type = eArgType::Number;

            // TODO: refactor this
            char temp_buf[128];
            memcpy(temp_buf, arg.str.data(), arg.str.length());
            temp_buf[arg.str.length()] = '\0';

            arg.val = strtod(temp_buf, nullptr);
        }
    }

    return !out_args.empty() && out_args[0].type == eArgType::String;
}

bool Eng::CmdlineUI::HandleInput(const Gui::input_event_t &ev, const std::vector<bool> &keys_state) {
    if (!enabled) {
        return false;
    }

    bool handled = false;
    if (ev.type == Gui::eInputEvent::KeyDown) {
        handled = true;
        if (ev.key_code == Eng::eKey::Grave) {
            enabled = !enabled;
        } else if (ev.key_code == Gui::eKey::Delete) {
            if (!history_.back().empty()) {
                history_.back().pop_back();
            }
        } else if (ev.key_code == Gui::eKey::Return) {
            command_to_execute_ = history_.back();

            history_.emplace_back();
            history_index_ = -1;
            if (history_.size() > MaxHistoryCount) {
                history_.erase(history_.begin());
            }
        } else if (ev.key_code == Gui::eKey::Tab) {
            Ren::String hint_str;
            NextHint(history_.back(), -1, hint_str);
            if (!hint_str.empty()) {
                history_.back() = hint_str.c_str();
            }
        } else if (ev.key_code == Gui::eKey::Grave) {
            if (!history_.back().empty()) {
                history_.emplace_back();
                // cmdline_history_index_ = -1;
                if (history_.size() > MaxHistoryCount) {
                    history_.erase(history_.begin());
                }
            }
        } else if (ev.key_code == Gui::eKey::Up) {
            history_index_ = std::min(++history_index_, int(history_.size()) - 2);
            history_.back() = history_[history_.size() - 2 - history_index_];
        } else if (ev.key_code == Gui::eKey::Down) {
            history_index_ = std::max(--history_index_, 0);
            history_.back() = history_[history_.size() - 2 - history_index_];
        } else {
            char ch = Eng::CharFromKeycode(ev.key_code);
            if (keys_state[eKey::LeftShift] || keys_state[eKey::RightShift]) {
                if (ch == '-') {
                    ch = '_';
                } else {
                    ch = toupper(ch);
                }
            }
            if (ch) {
                history_.back() += ch;
            } else {
                handled = false;
            }
        }
    }

    if (handled) {
        cursor_blink_us = 0;
        return true;
    }

    return BaseElement::HandleInput(ev, keys_state);
}

void Eng::CmdlineUI::Draw(Gui::Renderer *r) {
    if (!enabled) {
        return;
    }

    // background
    Gui::BaseElement::Draw(r);

    const float font_scale = 0.5f;
    const float font_height = font_.height(font_scale, this);

    // history
    float cur_y = SnapToPixels(Gui::Vec2f{0.0f, 0.0f + 0.1f * font_height})[1];
    for (int i = int(history_.size()) - 1; i >= 0 && cur_y < 1.0f; --i) {
        const float width = font_.DrawText(r, history_[i], Gui::Vec2f{-1, cur_y}, Gui::ColorWhite, font_scale, this);
        if (i == history_.size() - 1 && cursor_blink_us < 500000) {
            // draw cursor
            font_.DrawText(r, "_", Gui::Vec2f{-1.0f + width, cur_y}, Gui::ColorWhite, font_scale, this);
        }
        cur_y = SnapToPixels(Gui::Vec2f{0.0f, cur_y + font_height})[1];
    }

    // hints
    cur_y = SnapToPixels(Gui::Vec2f{0.0f, 0.0f - 1.0f * font_height})[1];
    if (!history_.empty() && !history_.back().empty()) {
        std::string_view cmd = history_.back();
        Ren::String hint_str;
        int index = NextHint(cmd, -1, hint_str);
        while (index != -1 && cur_y > -1.0f) {
            font_.DrawText(r, hint_str, Gui::Vec2f{-1.0f, cur_y}, Gui::ColorWhite, font_scale, this);
            cur_y -= font_height;
            index = NextHint(cmd, index, hint_str);
        }
    }
}