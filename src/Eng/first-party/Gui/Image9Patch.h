#pragma once

#include "Image.h"

namespace Gui {
class Image9Patch : public Image {
  protected:
    Vec2f offset_px_;
    float frame_scale_;

  public:
    Image9Patch(const Ren::TextureRegionRef &tex, const Vec2f &offset_px, float frame_scale, const Vec2f &pos,
                const Vec2f &size, const BaseElement *parent);
    Image9Patch(Ren::Context &ctx, std::string_view tex_name, const Vec2f &offset_px, float frame_scale,
                const Vec2f &pos, const Vec2f &size, const BaseElement *parent);

    const Ren::TextureRegionRef &tex() const { return tex_; }
    Ren::TextureRegionRef &tex() { return tex_; }

    void set_frame_scale(float scale) { frame_scale_ = scale; }

    void Draw(Renderer *r) override;
};
} // namespace Gui
