#pragma once

#include <Ren/Texture.h>
#include <Ren/TextureRegion.h>

#include "BaseElement.h"

namespace Gui {
class Image : public BaseElement {
  protected:
    Ren::TextureRegionRef tex_;
    Vec2f uvs_px_[2];

  public:
    Image(const Ren::TextureRegionRef &tex, const Vec2f &pos, const Vec2f &size, const BaseElement *parent);
    Image(Ren::Context &ctx, std::string_view tex_name, const Vec2f &pos, const Vec2f &size, const BaseElement *parent);

    Ren::TextureRegionRef &tex() { return tex_; }

    const Ren::TextureRegionRef &tex() const { return tex_; }

    const Vec2f *uvs_px() const { return uvs_px_; }

    void set_uvs(const Vec2f uvs[2]) {
        uvs_px_[0] = uvs[0];
        uvs_px_[1] = uvs[1];
    }

    void Draw(Renderer *r) override;

    void ResizeToContent(const Vec2f &pos, const BaseElement *parent);
};
} // namespace Gui
