#pragma once

#include <memory>

#include "ButtonBase.h"
#include "Image.h"

namespace Gui {
class ButtonImage : public ButtonBase {
  protected:
    Image image_normal_, image_focused_, image_pressed_;
    std::unique_ptr<BaseElement> additional_element_;

  public:
    ButtonImage(const Ren::ImageRegionRef &tex_normal, const Vec2f uvs_normal[2],
                const Ren::ImageRegionRef &tex_focused, const Vec2f uvs_focused[2],
                const Ren::ImageRegionRef &tex_pressed, const Vec2f uvs_pressed[2], const Vec2f &pos,
                const Vec2f &size, const BaseElement *parent);

    [[nodiscard]] BaseElement *element() const { return additional_element_.get(); }

    void SetElement(std::unique_ptr<BaseElement> &&el) {
        additional_element_ = std::move(el);
        additional_element_->Resize();
    }

    void Resize() override;

    void Draw(Renderer *r) override;
};
} // namespace Gui
