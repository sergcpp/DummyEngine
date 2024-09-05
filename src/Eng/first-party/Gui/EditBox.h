#pragma once

#include <bitset>

#include "Image9Patch.h"
#include "LinearLayout.h"

namespace Gui {
enum class eEditBoxFlags { Integers, Chars, Floats, Signed, Multiline };

class EditBox : public BaseElement {
    Image9Patch frame_;
    std::vector<std::string> lines_;
    const BitmapFont *font_;
    std::bitset<32> edit_flags_;
    bool focused_;
    int current_line_, current_char_;

  public:
    EditBox(Ren::Context &ctx, std::string_view frame_tex_name, const Vec2f &frame_offsets, const BitmapFont *font,
            const Vec2f &pos, const Vec2f &size, const BaseElement *parent);
    EditBox(Image9Patch frame, const BitmapFont *font, const Vec2f &pos, const Vec2f &size, const BaseElement *parent);

    Image9Patch &frame() { return frame_; }

    const std::string &line_text(int line) const { return lines_[line]; }

    bool focused() const { return focused_; }

    void set_focused(bool b) { focused_ = b; }

    void set_flag(eEditBoxFlags flag, bool enabled) { edit_flags_.set(size_t(flag), enabled); }

    void Resize(const BaseElement *parent) override;

    void Press(const Vec2f &p, bool push) override;

    void Draw(Renderer *r) override;

    int AddLine(std::string text);
    int InsertLine(std::string text);
    void DeleteLine(int line);

    void AddChar(int ch);
    void DeleteBck();
    void DeleteFwd();

    void MoveCursorH(int m);
    void MoveCursorV(int m);
};
} // namespace Gui
