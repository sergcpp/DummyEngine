#pragma once

#include "Image.h"

namespace Gui {
class Cursor : public BaseElement {
protected:
    Image		img_;
    bool		clicked_;
    Vec2f	    offset_;
public:
    Cursor(const Ren::Texture2DRef &tex, const Vec2f uvs[2], const Vec2f &size, const BaseElement *parent);
    Cursor(Ren::Context &ctx, const char *tex_name, const Vec2f uvs[2], const Vec2f &size, const BaseElement *parent);

    void set_clicked(bool b) {
        clicked_ = b;
    }
    void set_offset(const Vec2f &offset) {
        offset_ = offset;
    }

    void SetPos(const Vec2f &pos, const BaseElement *parent);

    void Draw(Renderer *r) override;
};
}

