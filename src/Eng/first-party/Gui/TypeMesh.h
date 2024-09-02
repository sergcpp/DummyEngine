#pragma once

#include <string>
#include <vector>

#include "BaseElement.h"

namespace Gui {
class TypeMesh : public BaseElement {
    std::string text_;
    std::vector<float> pos_, uvs_;
    std::vector<uint16_t> indices_;

    Vec2f center_;

    BitmapFont *font_;

  public:
    TypeMesh(const std::string &text, BitmapFont *font, const Vec2f &pos, const BaseElement *parent);

    const std::string &text() const { return text_; }
    const std::vector<float> &positions() const { return pos_; }
    const std::vector<float> &uvs() const { return uvs_; }

    void Centrate();

    void Move(const Vec2f &pos, const BaseElement *parent);

    void Resize(const BaseElement *parent) override;

    void Draw(Renderer *r) override;
};
} // namespace Gui
