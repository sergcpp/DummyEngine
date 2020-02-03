#include "Image9Patch.h"

#include <Ren/Context.h>

#include "Renderer.h"

Gui::Image9Patch::Image9Patch(
    const Ren::TextureRegionRef &tex, const Vec2f &offset_px, float frame_scale,
    const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
        : Image(tex, pos, size, parent), offset_px_(offset_px), frame_scale_(frame_scale) {
}

Gui::Image9Patch::Image9Patch(
    Ren::Context &ctx, const char *tex_name, const Vec2f &offset_px, float frame_scale,
    const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
        : Image(ctx, tex_name, pos, size, parent), offset_px_(offset_px), frame_scale_(frame_scale) {
}

void Gui::Image9Patch::Draw(Gui::Renderer *r) {
    const Ren::Texture2DParams &p = tex_->params();
    const int tex_layer = tex_->pos(2);

    const Vec2f offset_norm = offset_px_ * dims_[1] / Vec2f{ dims_px_[1] };

    const Vec2f pos[4] = {
        dims_[0],
        dims_[0] + dims_[1],
        dims_[0] + offset_norm,
        dims_[0] + dims_[1] - offset_norm
    };

    Vec2f uvs[4] = {
        Vec2f{ (float)(tex_->pos(0)),        (float)(tex_->pos(1)) },
        Vec2f{ (float)(tex_->pos(0) + p.w),  (float)(tex_->pos(1) + p.h) }
    };
    uvs[2] = uvs[0] + offset_px_;
    uvs[3] = uvs[1] - offset_px_;

    //  _______________
    // |   |       |   |
    // | 7 |   8   | 9 |
    // |___|_______|___|
    // |   |       |   |
    // | 4 |   5   | 6 |
    // |___|_______|___|
    // |   |       |   |
    // | 1 |   2   | 3 |
    // |___|_______|___|
    //

    // bottom part

    {   // 1
        const Vec2f
            _pos[2] = { pos[0], pos[2] },
            _uvs[2] = { Vec2f{ uvs[0][0], uvs[2][1] }, Vec2f{ uvs[2][0], uvs[0][1] } };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }

    {   // 2
        const Vec2f
            _pos[2] = { Vec2f{ pos[2][0], pos[0][1] }, Vec2f{ pos[3][0], pos[2][1] } },
            _uvs[2] = { Vec2f{ uvs[2][0], uvs[2][1] }, Vec2f{ uvs[3][0], uvs[0][1] } };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }

    {   // 3
        const Vec2f
            _pos[2] = { Vec2f{ pos[3][0], pos[0][1] }, Vec2f{ pos[1][0], pos[2][1] } },
            _uvs[2] = { Vec2f{ uvs[3][0], uvs[2][1] }, Vec2f{ uvs[1][0], uvs[0][1] } };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }

    // middle part

    {   // 4
        const Vec2f
            _pos[2] = { Vec2f{ pos[0][0], pos[2][1] }, Vec2f{ pos[2][0], pos[3][1] } },
            _uvs[2] = { Vec2f{ uvs[0][0], uvs[2][1] }, Vec2f{ uvs[2][0], uvs[3][1] } };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }

    {   // 5
        const Vec2f
            _pos[2] = { pos[2], pos[3] },
            _uvs[2] = { uvs[2], uvs[3] };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }

    {   // 6
        const Vec2f
            _pos[2] = { Vec2f{ pos[3][0], pos[2][1] }, Vec2f{ pos[1][0], pos[3][1] } },
            _uvs[2] = { Vec2f{ uvs[3][0], uvs[2][1] }, Vec2f{ uvs[1][0], uvs[3][1] } };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }

    // top part

    {   // 7
        const Vec2f
            _pos[2] = { Vec2f{ pos[0][0], pos[3][1] }, Vec2f{ pos[2][0], pos[1][1] } },
            _uvs[2] = { Vec2f{ uvs[0][0], uvs[1][1] }, Vec2f{ uvs[2][0], uvs[3][1] } };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }

    {   // 8
        const Vec2f
            _pos[2] = { Vec2f{ pos[2][0], pos[3][1] }, Vec2f{ pos[3][0], pos[1][1] } },
            _uvs[2] = { Vec2f{ uvs[2][0], uvs[1][1] }, Vec2f{ uvs[3][0], uvs[3][1] } };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }

    {   // 9
        const Vec2f
            _pos[2] = { pos[3], pos[1] },
            _uvs[2] = { Vec2f{ uvs[3][0], uvs[1][1] }, Vec2f{ uvs[1][0], uvs[3][1] } };
        r->DrawImageQuad(DrPassthrough, tex_layer, _pos, _uvs);
    }
}
