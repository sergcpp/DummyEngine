#include "TypeMesh.h"

#include "BitmapFont.h"
#include "Renderer.h"

Gui::TypeMesh::TypeMesh(const std::string &text, BitmapFont *font, const Vec2f &pos, const BaseElement *parent)
    : BaseElement(pos, {
    0, 0
}, parent),
text_(text), font_(font) {
    Move(pos, parent);
}

void Gui::TypeMesh::Centrate() {
    Vec2f delta = dims_[0] - center_;

    center_ += delta;
    dims_[0] += delta;

    Vec2i delta_px = Vec2i(delta * ((Vec2f)dims_px_[1] / dims_[1]));
    dims_px_[0] += delta_px;

    for (auto point = pos_.begin(); point != pos_.end(); point += 3) {
        point[0] += delta[0];
        point[1] += delta[1];
    }
}

void Gui::TypeMesh::Move(const Vec2f &pos, const BaseElement *parent) {
    float w = font_->GetTriangles(text_.c_str(), pos_, uvs_, indices_, pos, parent);
    Vec2f size = { w, font_->height(parent) };


    dims_[0] = parent->pos() + 0.5f * (pos + Vec2f(1, 1)) * parent->size();
    dims_[1] = size; //0.5f * rel_dims_[1] * parent->size();

    rel_dims_[0] = pos;
    rel_dims_[1] = 2.0f * dims_[1] / parent->size();

    dims_px_[0] = (Vec2i)((Vec2f)parent->pos_px() + 0.5f * (pos + Vec2f(1, 1)) * (Vec2f)parent->size_px());
    dims_px_[1] = (Vec2i)(size * (Vec2f)parent->size_px() * 0.5f);

    //dims_[0] = parent->pos() + 0.5f * (pos + vec2(1, 1)) * parent->size();
    //dims_[1] = size;

    center_ = dims_[0] + 0.5f * dims_[1];
}

void Gui::TypeMesh::Resize(const BaseElement *parent) {
    this->Move(rel_dims_[0], parent);
}

void Gui::TypeMesh::Draw(Renderer *r) {
    const Renderer::DrawParams &cur = r->GetParams();

    r->EmplaceParams(cur.col(), cur.z_val(), (eBlendMode)font_->blend_mode(), cur.scissor_test());
    r->DrawUIElement(font_->tex(), Gui::PRIM_TRIANGLE, pos_, uvs_, indices_);
    r->PopParams();
}