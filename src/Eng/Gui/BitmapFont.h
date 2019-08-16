#pragma once

#include <vector>

#include <Ren/Context.h>
#include <Ren/Texture.h>

#include "Renderer.h"
#include <Ren/MVec.h>

namespace Gui {
class BaseElement;
class Renderer;

using Ren::Vec2f;

struct typgraph_info_t {
    uint32_t line_height;
};
static_assert(sizeof(typgraph_info_t) == 4, "!");

struct glyph_info_t {
    int16_t pos[2];
    int8_t res[2];
    int8_t off[2];
    int8_t adv[2];
};
static_assert(sizeof(glyph_info_t) == 10, "!");

struct glyph_range_t {
    uint32_t beg, end;
};
static_assert(sizeof(glyph_range_t) == 8, "!");

enum class eFontFileChunk { FontChTypoData, FontChImageData, FontChGlyphData, FontChCount };

class BitmapFont {
  public:
    explicit BitmapFont(const char *name = nullptr, Ren::Context *ctx = nullptr);

    float scale() const { return scale_; }
    float height(const BaseElement *parent) const;
    eDrawMode draw_mode() const { return draw_mode_; }
    eBlendMode blend_mode() const { return blend_mode_; }
    Ren::TextureRegionRef tex() const { return tex_; }

    void set_scale(float scale) { scale_ = scale; }
    void set_draw_mode(eDrawMode mode) { draw_mode_ = mode; }

    bool Load(const char *name, Ren::Context &ctx);

    float GetWidth(const char *text, int text_len, const BaseElement *parent) const;
    float DrawText(Renderer *r, const char *text, const Vec2f &pos, const uint8_t col[4],
                   const BaseElement *parent) const;
    int CheckText(const char *text, const Vec2f &pos, const Vec2f &press_pos, float &out_char_offset,
                  const BaseElement *parent) const;

  private:
    typgraph_info_t info_;
    float scale_;
    Ren::TextureRegionRef tex_;
    uint32_t tex_res_[2];
    eDrawMode draw_mode_ = eDrawMode::Passthrough;
    eBlendMode blend_mode_ = eBlendMode::Alpha;
    std::unique_ptr<glyph_range_t[]> glyph_ranges_;
    uint32_t glyph_range_count_ = 0;
    uint32_t glyphs_count_ = 0;
    std::unique_ptr<glyph_info_t[]> glyphs_;
};

} // namespace Gui
