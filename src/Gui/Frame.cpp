#include "Frame.h"

#include <memory>

#include <Ren/Context.h>
#include <Sys/AssetFile.h>

#include "Renderer.h"

Gui::Frame::Frame(const Ren::Texture2DRef &tex, const Vec2f &offsets,
                  const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : BaseElement(pos, size, parent), tex_(tex), frame_offset_(offsets[0]), frame_offset_uv_(offsets[1]) {
}

Gui::Frame::Frame(Ren::Context &ctx, const char *tex_name, const Vec2f &offsets,
                  const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : BaseElement(pos, size, parent), frame_offset_(offsets[0]), frame_offset_uv_(offsets[1]) {

    Ren::eTexLoadStatus status;
    tex_ = ctx.LoadTexture2D(tex_name, nullptr, 0, {}, &status);
    if (status == Ren::TexCreatedDefault) {
        Sys::AssetFile in_file(tex_name, Sys::AssetFile::FileIn);
        size_t in_file_size = in_file.size();
        std::unique_ptr<char[]> data(new char[in_file_size]);
        in_file.Read(data.get(), in_file_size);

        Ren::Texture2DParams p;
        p.filter = Ren::Trilinear;
        p.repeat = Ren::Repeat;
        tex_ = ctx.LoadTexture2D(tex_name, data.get(), (int)in_file_size, p, &status);
        assert(status == Ren::TexCreatedFromData);
    }
    Resize(parent);
}

void Gui::Frame::Resize(const Gui::BaseElement *parent) {
    BaseElement::Resize(parent);

    const Vec2f off = 1.0f * Vec2f{ frame_offset_ } *dims_[1] / (Vec2f)dims_px_[1];
    const Vec2f min = dims_[0], max = dims_[0] + dims_[1];

    positions_ = { min[0], min[1], 0,
                   min[0] + off[0], min[1], 0,
                   min[0] + off[0], min[1] + off[1], 0,
                   min[0], min[1] + off[1], 0,

                   min[0] + off[0], max[1] - off[1], 0,
                   min[0], max[1] - off[1], 0,

                   max[0] - off[0], min[1] + off[1], 0,
                   max[0] - off[0], max[1] - off[1], 0,

                   max[0] - off[0], min[1], 0,

                   max[0], min[1], 0,
                   max[0], min[1] + off[1], 0,

                   max[0], max[1] - off[1], 0,

                   max[0], max[1], 0,
                   max[0] - off[0], max[1], 0,

                   min[0] + off[0], max[1], 0,
                   min[0], max[1], 0,
                 };

    uvs_ = { 0, 0,
             frame_offset_uv_, 0,
             frame_offset_uv_, frame_offset_uv_,
             0, frame_offset_uv_,

             frame_offset_uv_, 1 - frame_offset_uv_,
             0, 1 - frame_offset_uv_,

             1 - frame_offset_uv_, frame_offset_uv_,
             1 - frame_offset_uv_, 1 - frame_offset_uv_,

             1 - frame_offset_uv_, 0,

             1, 0,
             1, frame_offset_uv_,

             1, 1 - frame_offset_uv_,

             1, 1,
             1 - frame_offset_uv_, 1,

             frame_offset_uv_, 1,
             0, 1,
           };

    indices_ = { 0, 1, 2,
                 0, 2, 3,

                 2, 4, 3,
                 3, 4, 5,

                 2, 6, 7,
                 2, 7, 4,

                 1, 8, 6,
                 1, 6, 2,

                 8, 9, 10,
                 8, 10, 6,

                 6, 10, 11,
                 6, 11, 7,

                 7, 11, 12,
                 7, 12, 13,

                 4, 7, 13,
                 4, 13, 14,

                 5, 4, 14,
                 5, 14, 15,
               };
}

void Gui::Frame::Draw(Gui::Renderer *r) {
    r->DrawUIElement(tex_, PrimTriangle, positions_, uvs_, indices_);
}
