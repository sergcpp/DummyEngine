#pragma once

#include <Ren/Texture.h>

#include "BaseElement.h"

namespace Gui {
class Image : public BaseElement {
protected:
    Ren::Texture2DRef tex_;
    Vec2f             uvs_[2];
public:
    Image(const Ren::Texture2DRef &tex, const Vec2f uvs[2],
          const Vec2f &pos, const Vec2f &size, const BaseElement *parent);
    Image(Ren::Context &ctx, const char *tex_name, const Vec2f uvs[2],
          const Vec2f &pos, const Vec2f &size, const BaseElement *parent);

    void set_uvs(const Vec2f uvs[2]) {
        uvs_[0] = uvs[0];
        uvs_[1] = uvs[1];
    }

    void Draw(Renderer *r) override;
};
}

