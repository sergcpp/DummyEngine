#pragma once

#include <vector>

#include "../Ren/Context.h"
#include "../Ren/Texture.h"

#include "MVec.h"
#include "Renderer.h"

#undef DrawText

namespace Gui {
class BaseElement;
class Renderer;

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
    BitmapFont() = default;
    BitmapFont(std::string_view name, Ren::Context &ctx);
    BitmapFont(std::string_view name, std::istream &data, Ren::Context &ctx);

    [[nodiscard]] float default_scale() const { return default_scale_; }
    [[nodiscard]] float height(float scale, const BaseElement *parent) const;
    [[nodiscard]] float height(const BaseElement *parent) const { return height(default_scale_, parent); }
    [[nodiscard]] eDrawMode draw_mode() const { return draw_mode_; }
    [[nodiscard]] eBlendMode blend_mode() const { return blend_mode_; }
    [[nodiscard]] Ren::TextureRegionRef tex() const { return tex_; }

    void set_default_scale(const float scale) { default_scale_ = scale; }
    void set_draw_mode(const eDrawMode mode) { draw_mode_ = mode; }

    bool Load(std::string_view name, Ren::Context &ctx);
    bool Load(std::string_view name, std::istream &data, Ren::Context &ctx);

    float GetWidth(std::string_view text, float scale, const BaseElement *parent) const;
    float GetWidth(std::string_view text, const BaseElement *parent) const {
        return GetWidth(text, default_scale_, parent);
    }
    float DrawText(Renderer *r, std::string_view text, const Vec2f &pos, const uint8_t col[4], float scale,
                   const BaseElement *parent) const;
    float DrawText(Renderer *r, std::string_view text, const Vec2f &pos, const uint8_t col[4],
                   const BaseElement *parent) const {
        return DrawText(r, text, pos, col, default_scale_, parent);
    }
    int CheckText(std::string_view text, const Vec2f &pos, const Vec2f &press_pos, float scale, float &out_char_offset,
                  const BaseElement *parent) const;
    int CheckText(std::string_view text, const Vec2f &pos, const Vec2f &press_pos, float &out_char_offset,
                  const BaseElement *parent) const {
        return CheckText(text, pos, press_pos, default_scale_, out_char_offset, parent);
    }

  private:
    typgraph_info_t info_ = {};
    float default_scale_ = 1;
    Ren::TextureRegionRef tex_;
    uint32_t tex_res_[2] = {};
    eDrawMode draw_mode_ = eDrawMode::Passthrough;
    eBlendMode blend_mode_ = eBlendMode::Alpha;
    std::unique_ptr<glyph_range_t[]> glyph_ranges_;
    uint32_t glyph_range_count_ = 0;
    uint32_t glyphs_count_ = 0;
    std::unique_ptr<glyph_info_t[]> glyphs_;
};

} // namespace Gui
