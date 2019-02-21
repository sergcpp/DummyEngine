#pragma once

#include <string>

#include "ButtonBase.h"
#include "TypeMesh.h"

namespace Gui {
class ButtonText : public ButtonBase {
protected:
    TypeMesh type_mesh_;
public:
    ButtonText(const std::string &text, BitmapFont *font, const Vec2f &pos, const BaseElement *parent);

    bool Check(const Vec2f &p) const override;
    bool Check(const Vec2i &p) const override;

    void Move(const Vec2f &pos, const BaseElement *parent);

    void Draw(Renderer *r) override;
};
}

