#pragma once

#include <bitset>

#include "Bitmask.h"
#include "Image9Patch.h"
#include "LinearLayout.h"
#include "Signal.h"

namespace Gui {
enum class eEditBoxFlags { Integers, Chars, Floats, Signed, Multiline };

class EditBox : public BaseElement {
    Image9Patch frame_;
    std::vector<std::string> lines_;
    const BitmapFont *font_ = nullptr;
    int current_line_ = 0, current_char_ = 0;
    bool focused_ = false;

  public:
    Bitmask<eEditBoxFlags> edit_flags;

    Signal<void(std::string_view)> updated_signal;

    EditBox(Ren::Context &ctx, std::string_view frame_tex_name, const Vec2f &frame_offsets, const BitmapFont *font,
            const Vec2f &pos, const Vec2f &size, const BaseElement *parent);
    EditBox(Image9Patch frame, const BitmapFont *font, const Vec2f &pos, const Vec2f &size, const BaseElement *parent);

    Image9Patch &frame() { return frame_; }

    [[nodiscard]] std::string_view line_text(int line) const { return lines_[line]; }

    void Resize() override;

    void Draw(Renderer *r) override;

    bool HandleInput(const input_event_t &ev, const std::vector<bool> &keys_state) override;

    bool Press(const Vec2i &p, bool push);

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
