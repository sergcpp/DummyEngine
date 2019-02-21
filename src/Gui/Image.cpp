#include "Image.h"

#include <memory>

#include <Sys/AssetFile.h>

#include "Renderer.h"

Gui::Image::Image(const Ren::Texture2DRef &tex, const Vec2f uvs[2],
                  const Vec2f &pos, const Vec2f &size, const BaseElement *parent) :
    BaseElement(pos, size, parent), tex_(tex) {
    uvs_[0] = uvs[0];
    uvs_[1] = uvs[1];
}

Gui::Image::Image(Ren::Context &ctx, const char *tex_name, const Vec2f uvs[2],
                  const Vec2f &pos, const Vec2f &size, const BaseElement *parent) :
    BaseElement(pos, size, parent) {
    uvs_[0] = uvs[0];
    uvs_[1] = uvs[1];

    Ren::eTexLoadStatus status;
    tex_ = ctx.LoadTexture2D(tex_name, nullptr, 0, {}, &status);
    if (status == Ren::TexCreatedDefault) {
        Sys::AssetFile in_file(tex_name, Sys::AssetFile::IN);
        size_t in_file_size = in_file.size();
        std::unique_ptr<char[]> data(new char[in_file_size]);
        in_file.Read(data.get(), in_file_size);

        Ren::Texture2DParams p;
        p.filter = Ren::Trilinear;
        p.repeat = Ren::Repeat;
        tex_ = ctx.LoadTexture2D(tex_name, data.get(), (int)in_file_size, p, &status);
        assert(status == Ren::TexCreatedFromData);
    }
}

void Gui::Image::Draw(Renderer *r) {
    r->DrawImageQuad(tex_, dims_, uvs_);
}
