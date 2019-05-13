#pragma once

#include <vector>

#include <Ren/Texture.h>

#include "BaseElement.h"

namespace Gui {
class Frame : public BaseElement {
    Ren::Texture2DRef tex_;
    float frame_offset_, frame_offset_uv_;

    std::vector<float> positions_, uvs_;
    std::vector<uint16_t> indices_;
public:
    Frame(const Ren::Texture2DRef &tex, const Vec2f &offsets,
          const Vec2f &pos, const Vec2f &size, const BaseElement *parent);
    Frame(Ren::Context &ctx, const char *tex_name, const Vec2f &offsets,
          const Vec2f &pos, const Vec2f &size, const BaseElement *parent);

    Ren::Texture2DRef &tex() {
        return tex_;
    }

    void Resize(const BaseElement *parent) override;

    void Draw(Renderer *r) override;
};
}
