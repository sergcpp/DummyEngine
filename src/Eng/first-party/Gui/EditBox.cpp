#include "EditBox.h"

#include "BitmapFont.h"
#include "Renderer.h"
#include "Utils.h"

namespace Gui {
namespace EditBoxConstants {
const Bitmask<eEditBoxFlags> default_flags = Bitmask<eEditBoxFlags>{eEditBoxFlags::Integers} | eEditBoxFlags::Chars |
                                             eEditBoxFlags::Floats | eEditBoxFlags::Signed | eEditBoxFlags::Multiline;
} // namespace EditBoxConstants
} // namespace Gui

Gui::EditBox::EditBox(Ren::Context &ctx, std::string_view frame_tex_name, const Vec2f &frame_offsets,
                      const BitmapFont *font, const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : EditBox({ctx, frame_tex_name, frame_offsets, 1, Vec2f{-1}, Vec2f{2}, this}, font, pos, size, parent) {}

Gui::EditBox::EditBox(Image9Patch frame, const BitmapFont *font, const Vec2f &pos, const Vec2f &size,
                      const BaseElement *parent)
    : BaseElement(pos, size, parent), frame_(std::move(frame)), font_(font),
      edit_flags(EditBoxConstants::default_flags) {
    frame_.set_parent(this);
    frame_.Resize();
    lines_.emplace_back();
}

void Gui::EditBox::Resize() {
    BaseElement::Resize();

    frame_.Resize();
}

void Gui::EditBox::Draw(Renderer *r) {
    frame_.Draw(r);

    static const uint8_t color_white[] = {255, 255, 255, 255};

    const float font_height_orig = font_->height(1.0f, this);
    const float font_height = font_height_orig / dims_[1][1], line_spacing = 1.5f * font_height,
                x_start = -1.0f + 8.0f / dims_px_[1][0], y_start = 1.0f - 1.0f * font_height_orig;

    float cur_y = y_start;
    for (const std::string &line : lines_) {
        font_->DrawText(r, line, Vec2f{x_start, cur_y}, color_white, this);
        cur_y -= line_spacing;
    }

    if (focused_) {
        const float width_until_cursor =
                        2 * font_->GetWidth({lines_[current_line_].c_str(), size_t(current_char_)}, this),
                    y_offset = y_start - float(current_line_) * line_spacing;

        // draw cursor
        font_->DrawText(r, "|", Vec2f{x_start + width_until_cursor, y_offset}, color_white, this);
    }
}

bool Gui::EditBox::HandleInput(const input_event_t &ev, const std::vector<bool> &keys_state) {
    bool consumed = false;
    if (ev.type == eInputEvent::P1Down || ev.type == eInputEvent::P2Down) {
        consumed = Press(Vec2i(ev.point), true);
    } else if (ev.type == eInputEvent::P1Up || ev.type == eInputEvent::P2Up) {
        consumed = Press(Vec2i(ev.point), false);
    } else if (ev.type == eInputEvent::P1Move) {
        // consumed = Hover(ToLocal(Gui::Vec2i(ev.point)));
    } else if (ev.type == eInputEvent::KeyDown) {
        //input_processed = false;

        if (ev.key_code == eKey::LeftShift || ev.key_code == eKey::RightShift) {
        } else if (ev.key_code == eKey::Return) {
            InsertLine({});
        } else if (ev.key_code == eKey::Left) {
            MoveCursorH(-1);
        } else if (ev.key_code == eKey::Right) {
            MoveCursorH(1);
        } else if (ev.key_code == eKey::Up) {
            MoveCursorV(-1);
        } else if (ev.key_code == eKey::Down) {
            MoveCursorV(1);
        } else if (ev.key_code == eKey::Delete) {
            DeleteBck();
        } else if (ev.key_code == eKey::DeleteForward) {
            DeleteFwd();
        } else {
            char ch = CharFromKeycode(ev.key_code);
            if (keys_state[eKey::LeftShift] || keys_state[eKey::RightShift]) {
                if (ch == '-') {
                    ch = '_';
                } else {
                    ch = char(std::toupper(ch));
                }
            }
            AddChar(ch);
        }

        updated_signal.FireN(lines_[current_line_]);
    }

    if (consumed) {
        return true;
    }

    return BaseElement::HandleInput(ev, keys_state);
}

bool Gui::EditBox::Press(const Vec2i &p, bool push) {
    if (!Check(p)) {
        focused_ = false;
        return false;
    }

    if (push) {
        const Vec2f lp = ToLocal(p);

        focused_ = true;

        const float font_height = font_->height(this);
        float cur_y = 1.0f - font_height;
        for (int i = 0; i < int(lines_.size()); i++) {
            const std::string &line = lines_[i];

            if (p[1] > cur_y && p[1] < cur_y + font_height) {
                float char_offset = 0;
                const int intersected_char = font_->CheckText(line, Vec2f{-1, cur_y}, lp, char_offset, this);
                if (intersected_char != -1) {
                    current_line_ = i;
                    current_char_ = intersected_char;
                }
                break;
            }

            cur_y -= font_height;
        }
        return true;
    }

    return false;
}

int Gui::EditBox::AddLine(std::string text) {
    if (!(edit_flags & eEditBoxFlags::Multiline) && !lines_.empty()) {
        return -1;
    }
    lines_.emplace_back(std::move(text));

    return int(lines_.size()) - 1;
}

int Gui::EditBox::InsertLine(std::string text) {
    if (!(edit_flags & eEditBoxFlags::Multiline)) {
        return -1;
    }
    lines_.insert(lines_.begin() + current_line_ + 1, std::move(text));
    current_line_++;
    current_char_ = 0;

    return current_line_;
}

void Gui::EditBox::DeleteLine(int line) { lines_.erase(lines_.begin() + line); }

void Gui::EditBox::AddChar(int ch) {
    if (current_line_ >= int(lines_.size())) {
        return;
    }

    std::string &cur_line = lines_[current_line_];
    cur_line.insert(cur_line.begin() + current_char_, ch);

    current_char_++;

    /*if (current_line_ >= int(lines_.size())) return;

    switch (c) {
    case 191:
        c = '0' - 1;
        break;
    case 190:
        if (!(edit_flags_[Chars] || edit_flags_[Floats])) return;
        c = 46;
        break;
    case 188:
        if (!edit_flags_[Chars]) return;
        c = 44;
        break;
    default:
        if (((c == ' ' || (c >= 'A' && c <= 'z') || (c >= 160 && c <= 255)) && (edit_flags_[Chars])) ||
                ((c == '.' || c == '-' || c == '=' || c == '+' || c == '/' || c == '{' || c == '}' || (c >= '0' && c <=
    '9')) && (edit_flags_[Integers] || edit_flags_[Floats]))) { } else {
            //LOGE("No %i", int(s));
            return;
        }
    }

    std::string text = lines_[current_line_].text();
    text.insert(text.begin() + current_char_, (char)c);

    lines_[current_line_] = TypeMesh(text, font_, Vec2f{ 0 }, this);
    current_char_++;*/
}

void Gui::EditBox::DeleteBck() {
    if (current_line_ >= int(lines_.size())) {
        return;
    }

    std::string &line = lines_[current_line_];
    const int ch = current_char_ - 1;
    if (ch < 0 || ch >= int(line.length())) {
        return;
    }

    line.erase(ch, 1);
    current_char_--;
}

void Gui::EditBox::DeleteFwd() {
    if (current_line_ >= int(lines_.size())) {
        return;
    }

    std::string &line = lines_[current_line_];
    const int ch = current_char_;
    if (ch < 0 || ch >= int(line.length())) {
        return;
    }

    line.erase(ch, 1);
}

void Gui::EditBox::MoveCursorH(const int m) {
    const int line_len = CalcUTF8Length(lines_[current_line_].c_str());
    current_char_ = std::max(std::min(current_char_ + m, line_len), 0);
}

void Gui::EditBox::MoveCursorV(int m) {
    current_line_ = std::max(std::min(current_line_ + m, int(lines_.size()) - 1), 0);

    const int line_len = CalcUTF8Length(lines_[current_line_].c_str());
    current_char_ = std::max(std::min(current_char_, line_len), 0);
}
