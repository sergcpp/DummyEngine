#include "EditBox.h"

#include "Renderer.h"

namespace EditBoxConstants {
const unsigned long long default_flags =
    (1 << Gui::Integers) |
    (1 << Gui::Chars) |
    (1 << Gui::Floats) |
    (1 << Gui::Signed) |
    (1 << Gui::Multiline);

const int padding = 10;
const int cursor_offset = 12;
}

Gui::EditBox::EditBox(Ren::Context &ctx, const char *frame_tex_name, const Vec2f &frame_offsets,
                      BitmapFont *font,
                      const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : EditBox( {
    ctx, frame_tex_name, frame_offsets, Vec2f{ -1, -1 }, Vec2f{ 2, 2 }, this
}, font, pos, size, parent) {
}

Gui::EditBox::EditBox(const Frame &frame, BitmapFont *font,
                      const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : BaseElement(pos, size, parent),
      cursor_("|", font, {
    0, 0
}, this),
lay_(Vec2f{ -1 + 2.0f * EditBoxConstants::padding / parent->size_px()[0], -1 }, Vec2f{ 2, 2 }, this),
frame_(frame), font_(font), edit_flags_(EditBoxConstants::default_flags), focused_(false),
current_line_(0), current_char_(0) {
    lay_.set_vetical(true);
    frame_.Resize(this);
}

void Gui::EditBox::Resize(const BaseElement *parent) {
    BaseElement::Resize(parent);
    lay_.Resize(Vec2f{ -1 + 2.0f * EditBoxConstants::padding / parent->size_px()[0], -1 }, Vec2f{ 2, 2 }, this);
    frame_.Resize(this);

    UpdateCursor();
}

void Gui::EditBox::Press(const Vec2f &p, bool push) {
    if (!push) return;

    if (Check(p)) {
        focused_ = true;
        for (auto it = lines_.begin(); it != lines_.end(); ++it) {
            if (it->Check(p)) {
                current_line_ = (int)std::distance(lines_.begin(), it);
                const std::vector<float> &pos = it->positions();
                for (unsigned i = 0; i < pos.size(); i += 4 * 3) {
                    if ((i == 0 && p[0] > pos[i]) || (i > 0 && p[0] > 0.5f * (pos[i] + pos[i - 3]))) {
                        current_char_ = i / 12;
                    }
                }
                UpdateCursor();
                break;
            } else if (p[1] >= it->pos()[1] && p[1] <= it->pos()[1] + it->size()[1]) {
                current_line_ = (int)std::distance(lines_.begin(), it);
                current_char_ = (int)lines_[current_line_].text().length();
                UpdateCursor();
                break;
            }
        }
    } else {
        focused_ = false;
    }
}

void Gui::EditBox::Draw(Renderer *r) {
    const Renderer::DrawParams &cur = r->GetParams();
    r->EmplaceParams(cur.col_and_mode(), cur.z_val(), cur.blend_mode(), dims_px_);

    frame_.Draw(r);
    lay_.Draw(r);

    r->EmplaceParams(Vec4f(0.75f, 0.75f, 0.75f, 0.0f), cur.z_val(), cur.blend_mode(), dims_px_);
    if (focused_) {
        cursor_.Draw(r);
    }
    r->PopParams();

    r->PopParams();
}

int Gui::EditBox::AddLine(const std::string &text) {
    if (!edit_flags_[Multiline] && !lines_.empty()) return -1;

    lines_.emplace_back(text, font_, Vec2f{ 0, 0 }, this);

    // pointers could be invalidated after reallocation, so...
    UpdateLayout();

    return (int)lines_.size() - 1;
}

int Gui::EditBox::InsertLine(const std::string &text) {
    if (!edit_flags_[Multiline]) return -1;

    lines_.insert(lines_.begin() + current_line_, { text, font_, Vec2f{ 0, 0 }, this });

    UpdateLayout();

    return current_line_;
}

void Gui::EditBox::DeleteLine(unsigned line) {
    lines_.erase(lines_.begin() + line);
    UpdateLayout();
}

void Gui::EditBox::UpdateLayout() {
    lay_.Clear();
    for (TypeMesh &l : lines_) {
        lay_.AddElement(&l);
    }
    lay_.Resize(this);
}

void Gui::EditBox::UpdateCursor() {
    using namespace EditBoxConstants;

    if (current_line_ >= (int)lines_.size()) return;
    const TypeMesh &cur_line = lines_[current_line_];

    Vec2f cur_pos = { 0, cur_line.pos()[1] };
    if (current_char_ < (int)line_text(current_line_).length()) {
        cur_pos[0] = cur_line.positions()[current_char_ * 12];
    } else {
        cur_pos[0] = cur_line.pos()[0] + cur_line.size()[0];
    }

    cur_pos = 2.0f * (cur_pos - pos()) / size() - Vec2f(1, 1);
    cur_pos[0] -= float(cursor_offset) / size_px()[0];

    cursor_.Move(cur_pos, this);
}

void Gui::EditBox::AddChar(int c) {
    if (current_line_ >= (int)lines_.size()) return;

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
                ((c == '.' || c == '-' || c == '=' || c == '+' || c == '/' || c == '{' || c == '}' || (c >= '0' && c <= '9')) &&
                 (edit_flags_[Integers] || edit_flags_[Floats]))) {
        } else {
            //LOGE("No %i", (int)s);
            return;
        }
    }

    std::string text = lines_[current_line_].text();
    text.insert(text.begin() + current_char_, (char)c);

    lines_[current_line_] = TypeMesh(text, font_, { 0.0f, 0.0f }, this);
    current_char_++;

    UpdateLayout();
    UpdateCursor();
}

void Gui::EditBox::DeleteChar() {
    if (current_line_ >= (int)lines_.size()) return;

    std::string text = lines_[current_line_].text();

    int ch = current_char_ - 1;
    if (ch < 0 || ch >= (int)text.length()) return;
    text.erase(text.begin() + ch);

    lines_[current_line_] = TypeMesh(text, font_, { 0.0f, 0.0f }, this);
    current_char_--;

    UpdateLayout();
    UpdateCursor();
}

bool Gui::EditBox::MoveCursorH(int m) {
    if (current_line_ >= (int)lines_.size()) return false;

    current_char_ += m;

    int len = (int)lines_[current_line_].text().length();

    if (current_char_ < 0) {
        current_char_ = 0;
        return false;
    } else if (current_char_ > len) {
        current_char_ = len;
        return false;
    }

    UpdateCursor();

    return true;
}

bool Gui::EditBox::MoveCursorV(int m) {
    if (current_line_ >= (int)lines_.size()) return false;

    bool res = true;

    current_line_ += m;

    if (current_line_ < 0) {
        current_line_ = 0;
        res = false;
    } else if (current_line_ >= (int)lines_.size()) {
        current_line_ = (int)lines_.size() - 1;
        res = false;
    }

    int len = (int)lines_[current_line_].text().length();

    if (current_char_ < 0) {
        current_char_ = 0;
        return false;
    } else if (current_char_ > len) {
        current_char_ = len;
        return false;
    }

    UpdateCursor();

    return res;
}