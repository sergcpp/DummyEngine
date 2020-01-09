#include "BitmapFont.h"

#include <cassert>
#include <cstring>

#include <Sys/AssetFile.h>

#include "BaseElement.h"
#include "Renderer.h"
#include "Utils.h"

namespace BitmapFontInternal {
    
}

Gui::BitmapFont::BitmapFont(const char *name, Ren::Context *ctx) : info_{}, scale_(1.0f), tex_res_{} {
    if (name && ctx) {
        this->Load(name, *ctx);
    }
}

float Gui::BitmapFont::height(const BaseElement *parent) const {
    return scale_ * float(info_.line_height) * parent->size()[1] / parent->size_px()[1];
}

bool Gui::BitmapFont::Load(const char *fname, Ren::Context &ctx) {
    Sys::AssetFile in_file(fname, Sys::AssetFile::FileIn);
    if (!in_file) return false;

    size_t file_size = in_file.size();

    char sign[4];
    if (!in_file.Read(sign, 4) ||
        sign[0] != 'F' || sign[1] != 'O' || sign[2] != 'N' || sign[3] != 'T') {
        return false;
    }

    uint32_t header_size;
    if (!in_file.Read((char *)&header_size, sizeof(uint32_t))) {
        return false;
    }

    const uint32_t expected_chunks_size = int(Gui::FontChCount) * 3 * sizeof(uint32_t);
    const uint32_t chunks_size = header_size - 4 - sizeof(uint32_t);
    if (chunks_size != expected_chunks_size) return false;

    for (uint32_t i = 0; i < chunks_size; i += 3 * sizeof(uint32_t)) {
        uint32_t chunk_id, chunk_off, chunk_size;
        if (!in_file.Read((char *)&chunk_id, sizeof(uint32_t)) ||
            !in_file.Read((char *)&chunk_off, sizeof(uint32_t)) ||
            !in_file.Read((char *)&chunk_size, sizeof(uint32_t))) {
            return false;
        }

        const size_t old_pos = in_file.pos();
        in_file.Seek(chunk_off);

        if (chunk_id == Gui::FontChTypoData) {
            if (!in_file.Read((char *)&info_, sizeof(typgraph_info_t))) {
                return false;
            }
        } else if (chunk_id == Gui::FontChImageData) {
            uint16_t img_data_w, img_data_h;
            if (!in_file.Read((char *)&img_data_w, sizeof(uint16_t)) ||
                !in_file.Read((char *)&img_data_h, sizeof(uint16_t))) {
                return false;
            }

            tex_res_[0] = img_data_w;
            tex_res_[1] = img_data_h;

            uint16_t draw_mode, blend_mode;
            if (!in_file.Read((char *)&draw_mode, sizeof(uint16_t)) ||
                !in_file.Read((char *)&blend_mode, sizeof(uint16_t))) {
                return false;
            }

            draw_mode_ = (Gui::eDrawMode)draw_mode;
            blend_mode_ = (Gui::eBlendMode)blend_mode;

            const int img_data_size = 4 * img_data_w * img_data_h;
            std::unique_ptr<uint8_t[]> img_data(new uint8_t[img_data_size]);

            if (!in_file.Read((char *)img_data.get(), img_data_size)) {
                return false;
            }

            Ren::Texture2DParams p;
            p.w = img_data_w;
            p.h = img_data_h;
            p.filter = draw_mode_ == DrPassthrough ? Ren::NoFilter : Ren::BilinearNoMipmap;
            p.repeat = Ren::ClampToBorder;
            p.format = Ren::RawRGBA8888;

            tex_ = ctx.LoadTexture2D(fname, img_data.get(), img_data_size, p, nullptr);
            tex2_ = ctx.LoadTextureRegion(fname, img_data.get(), img_data_size, p, nullptr);
        } else if (chunk_id == Gui::FontChGlyphData) {
            if (!in_file.Read((char *)&glyph_range_count_, sizeof(uint32_t))) {
                return false;
            }

            glyph_ranges_.reset(new glyph_range_t[glyph_range_count_]);

            if (!in_file.Read((char *)glyph_ranges_.get(), glyph_range_count_ * sizeof(glyph_range_t))) {
                return false;
            }

            glyphs_count_ = 0;
            for (uint32_t j = 0; j < glyph_range_count_; j++) {
                glyphs_count_ += (glyph_ranges_[j].end - glyph_ranges_[j].beg);
            }

            glyphs_.reset(new glyph_info_t[glyphs_count_]);

            if (!in_file.Read((char *)glyphs_.get(), glyphs_count_ * sizeof(glyph_info_t))) {
                return false;
            }
        }

        in_file.Seek(old_pos);
    }

    return true;
}

float Gui::BitmapFont::GetWidth(const char *text, const BaseElement *parent) const {
    const Vec2f
        m = scale_ * parent->size() / (Vec2f)parent->size_px();

    int cur_x = 0, cur_y = 0;

    int char_pos = 0;
    while (text[char_pos]) {
        uint32_t unicode;
        char_pos += Gui::ConvChar_UTF8_to_Unicode(&text[char_pos], unicode);

        uint32_t glyph_index = 0;
        for (uint32_t i = 0; i < glyph_range_count_; i++) {
            const glyph_range_t &rng = glyph_ranges_[i];

            if (unicode >= rng.beg && unicode < rng.end) {
                glyph_index += (unicode - rng.beg);
                break;
            } else {
                glyph_index += (rng.end - rng.beg);
            }
        }
        assert(glyph_index < glyphs_count_);

        const glyph_info_t &glyph = glyphs_[glyph_index];
        cur_x += glyph.adv[0];
    }

    return float(cur_x) * m[0];
}

float Gui::BitmapFont::PrepareVertexData(const char *text, const Vec2f &pos, const uint8_t col[4], const BaseElement *parent,
                                         std::vector<vertex_t> &vtx_data, std::vector<uint16_t> &ndx_data) const {
    using namespace BitmapFontInternal;

    const Vec2f
        p = parent->pos() + 0.5f * (pos + Vec2f(1, 1)) * parent->size(),
        m = scale_ * parent->size() / (Vec2f)parent->size_px();

    const uint16_t
        uvs_offset[2] = { (uint16_t)tex2_->pos(0), (uint16_t)tex2_->pos(1) },
        tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex2_->pos(2)));

    int cur_x = 0;

    const Vec2f uvs_scale = 1.0f / Vec2f{ (float)Ren::TextureAtlasWidth, (float)Ren::TextureAtlasHeight };

    int char_pos = 0;
    while (text[char_pos]) {
        uint32_t unicode;
        char_pos += Gui::ConvChar_UTF8_to_Unicode(&text[char_pos], unicode);

        uint32_t glyph_index = 0;
        for (uint32_t i = 0; i < glyph_range_count_; i++) {
            const glyph_range_t &rng = glyph_ranges_[i];

            if (unicode >= rng.beg && unicode < rng.end) {
                glyph_index += (unicode - rng.beg);
                break;
            } else {
                glyph_index += (rng.end - rng.beg);
            }
        }
        assert(glyph_index < glyphs_count_);

        const glyph_info_t &glyph = glyphs_[glyph_index];
        if (glyph.res[0]) {
            uint16_t ndx_offset = (uint16_t)vtx_data.size();

            vtx_data.resize(vtx_data.size() + 4);
            ndx_data.resize(ndx_data.size() + 6);

            vertex_t *cur_vtx = &vtx_data[vtx_data.size() - 4];
            uint16_t *cur_ndx = &ndx_data[ndx_data.size() - 6];

            cur_vtx->pos[0] = p[0] + float(cur_x + glyph.off[0] - 1) * m[0];
            cur_vtx->pos[1] = p[1] + float(glyph.off[1] - 1) * m[1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] - 1));
            cur_vtx->uvs[1] = f32_to_u16(uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] + glyph.res[1] + 1));
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = draw_mode_ == DrDistanceField ? 65535 : 0;
            ++cur_vtx;

            cur_vtx->pos[0] = p[0] + float(cur_x + glyph.off[0] + glyph.res[0] + 1) * m[0];
            cur_vtx->pos[1] = p[1] + float(glyph.off[1] - 1) * m[1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] + glyph.res[0] + 1));
            cur_vtx->uvs[1] = f32_to_u16(uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] + glyph.res[1] + 1));
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = draw_mode_ == DrDistanceField ? 65535 : 0;
            ++cur_vtx;

            cur_vtx->pos[0] = p[0] + float(cur_x + glyph.off[0] + glyph.res[0] + 1) * m[0];
            cur_vtx->pos[1] = p[1] + float(glyph.off[1] + glyph.res[1] + 1) * m[1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] + glyph.res[0] + 1));
            cur_vtx->uvs[1] = f32_to_u16(uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] - 1));
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = draw_mode_ == DrDistanceField ? 65535 : 0;
            ++cur_vtx;

            cur_vtx->pos[0] = p[0] + float(cur_x + glyph.off[0] - 1) * m[0];
            cur_vtx->pos[1] = p[1] + float(glyph.off[1] + glyph.res[1] + 1) * m[1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] - 1));
            cur_vtx->uvs[1] = f32_to_u16(uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] - 1));
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = draw_mode_ == DrDistanceField ? 65535 : 0;
            ++cur_vtx;

            (*cur_ndx++) = ndx_offset + 0;
            (*cur_ndx++) = ndx_offset + 1;
            (*cur_ndx++) = ndx_offset + 2;

            (*cur_ndx++) = ndx_offset + 0;
            (*cur_ndx++) = ndx_offset + 2;
            (*cur_ndx++) = ndx_offset + 3;

            ndx_offset += 4;
        }

        cur_x += glyph.adv[0];
    }

    return float(cur_x) * m[0];
}

float Gui::BitmapFont::DrawText(Renderer *r, const char *text, const Vec2f &pos, const uint8_t col[4], const BaseElement *parent) const {
    using namespace BitmapFontInternal;

    const Vec2f
            p = parent->pos() + 0.5f * (pos + Vec2f(1, 1)) * parent->size(),
            m = scale_ * parent->size() / (Vec2f)parent->size_px();

    const uint16_t
        uvs_offset[2] = { (uint16_t)tex2_->pos(0), (uint16_t)tex2_->pos(1) },
        tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex2_->pos(2)));

    vertex_t *vtx_data; int vtx_avail = 0;
    uint16_t *ndx_data; int ndx_avail = 0;
    int ndx_offset = r->AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    int cur_x = 0;

    const Vec2f uvs_scale = 1.0f / Vec2f{ (float)Ren::TextureAtlasWidth, (float)Ren::TextureAtlasHeight };

    int char_pos = 0;
    while(text[char_pos]) {
        uint32_t unicode;
        char_pos += Gui::ConvChar_UTF8_to_Unicode(&text[char_pos], unicode);

        uint32_t glyph_index = 0;
        for (uint32_t i = 0; i < glyph_range_count_; i++) {
            const glyph_range_t &rng = glyph_ranges_[i];

            if (unicode >= rng.beg && unicode < rng.end) {
                glyph_index += (unicode - rng.beg);
                break;
            } else {
                glyph_index += (rng.end - rng.beg);
            }
        }
        assert(glyph_index < glyphs_count_);

        const glyph_info_t &glyph = glyphs_[glyph_index];
        if (glyph.res[0]) {
            vtx_avail -= 4;
            ndx_avail -= 6;
            if (vtx_avail < 0 || ndx_avail < 0) {
                r->SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data), true);
                // acquire new buffer
                ndx_offset = r->AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);

                cur_vtx = vtx_data;
                cur_ndx = ndx_data;
            }

            cur_vtx->pos[0] = p[0] + float(cur_x + glyph.off[0] - 1) * m[0];
            cur_vtx->pos[1] = p[1] + float(glyph.off[1] - 1) * m[1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] - 1));
            cur_vtx->uvs[1] = f32_to_u16(uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] + glyph.res[1] + 1));
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = draw_mode_ == DrDistanceField ? 65535 : 0;
            ++cur_vtx;

            cur_vtx->pos[0] = p[0] + float(cur_x + glyph.off[0] + glyph.res[0] + 1) * m[0];
            cur_vtx->pos[1] = p[1] + float(glyph.off[1] - 1) * m[1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] + glyph.res[0] + 1));
            cur_vtx->uvs[1] = f32_to_u16(uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] + glyph.res[1] + 1));
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = draw_mode_ == DrDistanceField ? 65535 : 0;
            ++cur_vtx;

            cur_vtx->pos[0] = p[0] + float(cur_x + glyph.off[0] + glyph.res[0] + 1) * m[0];
            cur_vtx->pos[1] = p[1] + float(glyph.off[1] + glyph.res[1] + 1) * m[1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] + glyph.res[0] + 1));
            cur_vtx->uvs[1] = f32_to_u16(uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] - 1));
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = draw_mode_ == DrDistanceField ? 65535 : 0;
            ++cur_vtx;

            cur_vtx->pos[0] = p[0] + float(cur_x + glyph.off[0] - 1) * m[0];
            cur_vtx->pos[1] = p[1] + float(glyph.off[1] + glyph.res[1] + 1) * m[1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] - 1));
            cur_vtx->uvs[1] = f32_to_u16(uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] - 1));
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = draw_mode_ == DrDistanceField ? 65535 : 0;
            ++cur_vtx;

            (*cur_ndx++) = ndx_offset + 0;
            (*cur_ndx++) = ndx_offset + 1;
            (*cur_ndx++) = ndx_offset + 2;

            (*cur_ndx++) = ndx_offset + 0;
            (*cur_ndx++) = ndx_offset + 2;
            (*cur_ndx++) = ndx_offset + 3;

            ndx_offset += 4;
        }

        cur_x += glyph.adv[0];
    }

    r->SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data), false);

    return float(cur_x) * m[0];
}