#include "Image.h"

#include <memory>

#include <Sys/AssetFile.h>

#include "Renderer.h"

Gui::Image::Image(const Ren::TextureRegionRef &tex, const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
        : BaseElement(pos, size, parent), tex_(tex) {
    const Ren::Texture2DParams &p = tex->params();
    uvs_px_[0] = Vec2f{ (float)(tex_->pos(0)),        (float)(tex_->pos(1)) };
    uvs_px_[1] = Vec2f{ (float)(tex_->pos(0) + p.w),  (float)(tex_->pos(1) + p.h) };
}

Gui::Image::Image(Ren::Context &ctx, const char *tex_name, const Vec2f &pos, const Vec2f &size, const BaseElement *parent) :
    BaseElement(pos, size, parent) {
    uvs_px_[0] = Vec2f{ 0.0f, 0.0f };
    uvs_px_[1] = Vec2f{ 0.0f, 0.0f };

    Ren::eTexLoadStatus status;
    tex_ = ctx.LoadTextureRegion(tex_name, nullptr, 0, {}, &status);
    if (status == Ren::eTexLoadStatus::TexCreatedDefault) {
        Sys::AssetFile in_file(tex_name, Sys::AssetFile::FileIn);
        size_t in_file_size = in_file.size();
        std::unique_ptr<char[]> data(new char[in_file_size]);
        in_file.Read(data.get(), in_file_size);

        tex_ = ctx.LoadTextureRegion(tex_name, data.get(), (int)in_file_size, {}, &status);
        assert(status == Ren::eTexLoadStatus::TexCreatedFromData);

        const Ren::Texture2DParams &p = tex_->params();
        uvs_px_[0] = Vec2f{ (float)(tex_->pos(0)),        (float)(tex_->pos(1)) };
        uvs_px_[1] = Vec2f{ (float)(tex_->pos(0) + p.w),  (float)(tex_->pos(1) + p.h) };
    }
}

void Gui::Image::Draw(Renderer *r) {
    const Vec2f pos[2] = {
        dims_[0],
        dims_[0] + dims_[1]
    };

    const Ren::Texture2DParams &p = tex_->params();
    const int tex_layer = tex_->pos(2);

    r->PushImageQuad(eDrawMode::DrPassthrough, tex_layer, pos, uvs_px_);
}

void Gui::Image::ResizeToContent(const Vec2f &pos, const BaseElement *parent) {
    const Ren::Texture2DParams &p = tex_->params();
    const Vec2i parent_size_px = parent->size_px();

    BaseElement::Resize(pos,
        Vec2f{ 2.0f * float(p.w) / float(parent_size_px[0]),
               2.0f * float(p.h) / float(parent_size_px[1]) }, parent);
}
