#include "Cursor.h"

#include "Renderer.h"

Gui::Cursor::Cursor(const Ren::Texture2DRef &tex, const Vec2f uvs[2], const Vec2f &size, const BaseElement *parent)
    : BaseElement(Vec2f(0, 0), size, parent),
      img_(tex, uvs, Vec2f(-1, -1), Vec2f(2, 2), this), clicked_(false) {
}

Gui::Cursor::Cursor(Ren::Context &ctx, const char *tex_name, const Vec2f uvs[2], const Vec2f &size, const BaseElement *parent)
    : BaseElement(Vec2f(0, 0), size, parent),
      img_(ctx, tex_name, uvs, Vec2f(-1, -1), Vec2f(2, 2), this), clicked_(false) {
}

void Gui::Cursor::SetPos(const Vec2f &pos, const BaseElement *parent) {
    Resize(pos, dims_[1], parent);
    if (clicked_) {
        img_.Resize(Vec2f(-1, -1) + offset_ * 0.8f, Vec2f(2, 2) * 0.8f, this);
    } else {
        img_.Resize(Vec2f(-1, -1) + offset_, Vec2f(2, 2), this);
    }
}

void Gui::Cursor::Draw(Renderer *r) {
    const Renderer::DrawParams &cur = r->GetParams();

    r->EmplaceParams(clicked_ ? Vec4f(0.8f, 0.8f, 0.8f, 0.0f) : Vec4f(1.0f, 1.0f, 1.0f, 0.0f),
                     cur.z_val(), cur.blend_mode(), cur.scissor_test());
    img_.Draw(r);
    r->PopParams();
}
