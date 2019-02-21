#pragma once

#include <vector>

#include <Ren/Context.h>
#include <Ren/Texture.h>

#include <Ren/MVec.h>

namespace Gui {
class BaseElement;
class Renderer;

using Ren::Vec2f;

class BitmapFont {
public:
    BitmapFont(const char *name = nullptr, Ren::Context *ctx = nullptr);

    float height(const BaseElement *parent) const;

    Ren::Texture2DRef tex() {
        return tex_;
    }

    int blend_mode() const;

    void set_scale(float s) {
        scale_ = s;
    }

    void set_sharp(bool b);

    bool Load(const char *name, Ren::Context &ctx);

    void SetCursor(int x, int y);

    void ReverseYAxis(bool state);

    float GetTriangles(const char *text, std::vector<float> &positions, std::vector<float> &uvs,
                       std::vector<unsigned char> &indices, const Vec2f &pos, const BaseElement *parent);

    float GetWidth(const char *text, const BaseElement *parent);

    void DrawText(Renderer *r, const char *text, const Vec2f &pos, const BaseElement *parent);

private:
    int cell_x_, cell_y_, y_offset_, row_pitch_;
    char base_;
    char width_[256];
    Ren::Texture2DRef tex_;
    int cur_x_, cur_y_;
    float row_factor_, col_factor_;
    int render_style_;
    float r_, g_, b_;
    bool invert_y_;
    float scale_;

    static std::vector<float> std_positions, std_uvs;
    static std::vector<unsigned char> std_indices;
};
}
