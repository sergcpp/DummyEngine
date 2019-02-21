#include "ButtonText.h"

#include "BitmapFont.h"
#include "Renderer.h"

Gui::ButtonText::ButtonText(const std::string &text, BitmapFont *font, const Vec2f &pos, const BaseElement *parent)
    : ButtonBase(pos, {
    0, 0
}, parent),
type_mesh_(text, font, pos, parent) {
}

bool Gui::ButtonText::Check(const Vec2f &p) const {
    return type_mesh_.Check(p);
}

bool Gui::ButtonText::Check(const Vec2i &p) const {
    return type_mesh_.Check(p);
}

void Gui::ButtonText::Move(const Vec2f &pos, const BaseElement *parent) {
    type_mesh_.Move(pos, parent);
}

void Gui::ButtonText::Draw(Renderer *r) {
    const auto &cur = r->GetParams();

    if (state_ == ST_NORMAL) {
        r->EmplaceParams(Vec3f(0.9f, 0.9f, 0.9f), cur.z_val(), cur.blend_mode(), cur.scissor_test());
    } else if (state_ == ST_FOCUSED) {
        r->EmplaceParams(Vec3f(1, 1, 1), cur.z_val(), cur.blend_mode(), cur.scissor_test());
    } else { // state_ == ST_PRESSED
        r->EmplaceParams(Vec3f(0.5f, 0.5f, 0.5f), cur.z_val(), cur.blend_mode(), cur.scissor_test());
    }
    type_mesh_.Draw(r);

    r->PopParams();
}

