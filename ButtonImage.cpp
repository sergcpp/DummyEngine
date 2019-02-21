#include "ButtonImage.h"

Gui::ButtonImage::ButtonImage(Ren::Context &ctx,
                              const char *tex_normal, const Vec2f uvs_normal[2],
                              const char *tex_focused, const Vec2f uvs_focused[2],
                              const char *tex_pressed, const Vec2f uvs_pressed[2],
                              const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : ButtonBase(pos, size, parent),
      image_normal_{ ctx, tex_normal, uvs_normal, Vec2f{-1, -1}, Vec2f{2, 2}, this },
      image_focused_{ ctx, tex_focused, uvs_focused, Vec2f{-1, -1}, Vec2f{2, 2}, this },
      image_pressed_{ ctx, tex_pressed, uvs_pressed, Vec2f{-1, -1}, Vec2f{2, 2}, this } {
}

void Gui::ButtonImage::Resize(const BaseElement *parent) {
    BaseElement::Resize(parent);

    image_normal_.Resize(this);
    image_focused_.Resize(this);
    image_pressed_.Resize(this);

    if (additional_element_) {
        additional_element_->Resize(this);
    }
}

void Gui::ButtonImage::Draw(Renderer *r) {
    if (state_ == ST_NORMAL) {
        image_normal_.Draw(r);
    } else if (state_ == ST_FOCUSED) {
        image_focused_.Draw(r);
    } else if (state_ == ST_PRESSED) {
        image_pressed_.Draw(r);
    }

    if (additional_element_) {
        additional_element_->Draw(r);
    }
}