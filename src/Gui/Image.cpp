#include "Image.h"

#include <memory>

#include <Sys/AssetFile.h>

#include "Renderer.h"

Gui::Image::Image(const Ren::TextureRegionRef &tex, const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
        : BaseElement(pos, size, parent), tex_(tex) {
    const Ren::Texture2DParams &p = tex->params();
    uvs_px_[0] = { (float)(tex_->pos(0)),        (float)(tex_->pos(1)) };
    uvs_px_[1] = { (float)(tex_->pos(0) + p.w),  (float)(tex_->pos(1) + p.h) };
}

Gui::Image::Image(Ren::Context &ctx, const char *tex_name, const Vec2f &pos, const Vec2f &size, const BaseElement *parent) :
    BaseElement(pos, size, parent) {
    uvs_px_[0] = { 0.0f, 0.0f };
    uvs_px_[1] = { 0.0f, 0.0f };

    Ren::eTexLoadStatus status;
    tex_ = ctx.LoadTextureRegion(tex_name, nullptr, 0, {}, &status);
    if (status == Ren::TexCreatedDefault) {
        Sys::AssetFile in_file(tex_name, Sys::AssetFile::FileIn);
        size_t in_file_size = in_file.size();
        std::unique_ptr<char[]> data(new char[in_file_size]);
        in_file.Read(data.get(), in_file_size);

        tex_ = ctx.LoadTextureRegion(tex_name, data.get(), (int)in_file_size, {}, &status);
        assert(status == Ren::TexCreatedFromData);

        const Ren::Texture2DParams &p = tex_->params();
        uvs_px_[0] = { (float)(tex_->pos(0)),        (float)(tex_->pos(1)) };
        uvs_px_[1] = { (float)(tex_->pos(0) + p.w),  (float)(tex_->pos(1) + p.h) };
    }
}

void Gui::Image::Draw(Renderer *r) {
    const Vec2f pos[2] = {
        dims_[0],
        dims_[0] + dims_[1]
    };

    const Ren::Texture2DParams &p = tex_->params();
    const int tex_layer = tex_->pos(2);

    r->DrawImageQuad(DrPassthrough, tex_layer, pos, uvs_px_);
}
