#include "Image.h"

#include <fstream>
#include <memory>

#include "../Ren/Context.h"

#include "Renderer.h"

Gui::Image::Image(const Ren::TextureRegionRef &tex, const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : BaseElement(pos, size, parent), tex_(tex) {
    const Ren::Tex2DParams &p = tex->params;
    uvs_px_[0] = Vec2f{float(tex_->pos(0)), float(tex_->pos(1))};
    uvs_px_[1] = Vec2f{float(tex_->pos(0) + p.w), float(tex_->pos(1) + p.h)};
}

Gui::Image::Image(Ren::Context &ctx, std::string_view tex_name, const Vec2f &pos, const Vec2f &size,
                  const BaseElement *parent)
    : BaseElement(pos, size, parent) {
    uvs_px_[0] = Vec2f{0};
    uvs_px_[1] = Vec2f{0};

    Ren::eTexLoadStatus status;
    tex_ = ctx.LoadTextureRegion(tex_name, {}, {}, &status);
    if (status == Ren::eTexLoadStatus::CreatedDefault) {
        std::ifstream in_file(tex_name.data(), std::ios::binary | std::ios::ate);
        const size_t in_file_size = size_t(in_file.tellg());
        in_file.seekg(0, std::ios::beg);
        std::vector<uint8_t> data(in_file_size);
        in_file.read((char *)data.data(), std::streamsize(in_file_size));

        tex_ = ctx.LoadTextureRegion(tex_name, data, {}, &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData);

        const Ren::Tex2DParams &p = tex_->params;
        uvs_px_[0] = Vec2f{float(tex_->pos(0)), float(tex_->pos(1))};
        uvs_px_[1] = Vec2f{float(tex_->pos(0) + p.w), float(tex_->pos(1) + p.h)};
    }
}

void Gui::Image::Draw(Renderer *r) {
    const Vec2f pos[2] = {dims_[0], dims_[0] + dims_[1]};

    //const Ren::Tex2DParams &p = tex_->params();
    const int tex_layer = tex_->pos(2);

    r->PushImageQuad(eDrawMode::Passthrough, tex_layer, ColorWhite, pos, uvs_px_);
}

void Gui::Image::ResizeToContent(const Vec2f &pos) {
    const Ren::Tex2DParams &p = tex_->params;
    const Vec2i parent_size_px = parent_->size_px();

    BaseElement::Resize(
        pos, Vec2f{2 * float(p.w) / float(parent_size_px[0]), 2 * float(p.h) / float(parent_size_px[1])});
}
