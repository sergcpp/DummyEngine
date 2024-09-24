#include "ButtonImage.h"

Gui::ButtonImage::ButtonImage(const Ren::TextureRegionRef &tex_normal, const Vec2f uvs_normal[2],
                              const Ren::TextureRegionRef &tex_focused, const Vec2f uvs_focused[2],
                              const Ren::TextureRegionRef &tex_pressed, const Vec2f uvs_pressed[2], const Vec2f &pos,
                              const Vec2f &size, const BaseElement *parent)
    : ButtonBase(pos, size, parent), image_normal_{tex_normal, Vec2f{-1}, Vec2f{2}, this},
      image_focused_{tex_focused, Vec2f{-1}, Vec2f{2}, this},
      image_pressed_{tex_pressed, Vec2f{-1}, Vec2f{2}, this} {
    image_normal_.set_uvs(uvs_normal);
    image_focused_.set_uvs(uvs_focused);
    image_pressed_.set_uvs(uvs_pressed);
}

void Gui::ButtonImage::Resize() {
    BaseElement::Resize();

    image_normal_.Resize();
    image_focused_.Resize();
    image_pressed_.Resize();

    if (additional_element_) {
        additional_element_->Resize();
    }
}

void Gui::ButtonImage::Draw(Renderer *r) {
    if (state_ == eState::Normal) {
        image_normal_.Draw(r);
    } else if (state_ == eState::Focused) {
        image_focused_.Draw(r);
    } else if (state_ == eState::Pressed) {
        image_pressed_.Draw(r);
    }

    if (additional_element_) {
        additional_element_->Draw(r);
    }
}