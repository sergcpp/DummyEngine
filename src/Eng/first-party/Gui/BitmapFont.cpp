#include "BitmapFont.h"

#include <cassert>
#include <cstring>
#include <fstream>

#include "BaseElement.h"
#include "Renderer.h"
#include "Utils.h"

Gui::BitmapFont::BitmapFont(std::string_view name, Ren::Context &ctx) : info_{}, tex_res_{} {
    if (!this->Load(name, ctx)) {
        throw std::runtime_error("Failed to load!");
    }
}

Gui::BitmapFont::BitmapFont(std::string_view name, std::istream &data, Ren::Context &ctx) {
    if (!this->Load(name, data, ctx)) {
        throw std::runtime_error("Failed to load!");
    }
}

float Gui::BitmapFont::height(const float scale, const BaseElement *parent) const {
    return 2.0f * scale * float(info_.line_height) / parent->size_px()[1];
}

bool Gui::BitmapFont::Load(std::string_view name, Ren::Context &ctx) {
    std::ifstream in_file(name.data(), std::ios::binary);
    if (!in_file) {
        return false;
    }
    return Load(name, in_file, ctx);
}

bool Gui::BitmapFont::Load(std::string_view name, std::istream &data, Ren::Context &ctx) {
    char sign[4];
    if (!data.read(sign, 4) || sign[0] != 'F' || sign[1] != 'O' || sign[2] != 'N' || sign[3] != 'T') {
        return false;
    }

    uint32_t header_size;
    if (!data.read((char *)&header_size, sizeof(uint32_t))) {
        return false;
    }

    const uint32_t expected_chunks_size = uint32_t(eFontFileChunk::FontChCount) * 3 * sizeof(uint32_t);
    const uint32_t chunks_size = header_size - 4 - sizeof(uint32_t);
    if (chunks_size != expected_chunks_size) {
        return false;
    }

    for (uint32_t i = 0; i < chunks_size; i += 3 * sizeof(uint32_t)) {
        uint32_t chunk_id, chunk_off, chunk_size;
        if (!data.read((char *)&chunk_id, sizeof(uint32_t)) || !data.read((char *)&chunk_off, sizeof(uint32_t)) ||
            !data.read((char *)&chunk_size, sizeof(uint32_t))) {
            return false;
        }

        const size_t old_pos = size_t(data.tellg());
        data.seekg(chunk_off, std::ios::beg);

        if (chunk_id == uint32_t(eFontFileChunk::FontChTypoData)) {
            if (!data.read((char *)&info_, sizeof(typgraph_info_t))) {
                return false;
            }
        } else if (chunk_id == uint32_t(eFontFileChunk::FontChImageData)) {
            uint16_t img_data_w, img_data_h;
            if (!data.read((char *)&img_data_w, sizeof(uint16_t)) ||
                !data.read((char *)&img_data_h, sizeof(uint16_t))) {
                return false;
            }

            tex_res_[0] = img_data_w;
            tex_res_[1] = img_data_h;

            uint16_t draw_mode, blend_mode;
            if (!data.read((char *)&draw_mode, sizeof(uint16_t)) || !data.read((char *)&blend_mode, sizeof(uint16_t))) {
                return false;
            }

            draw_mode_ = eDrawMode(draw_mode);
            blend_mode_ = eBlendMode(blend_mode);

            Ren::StageBufRef sb = ctx.default_stage_bufs().GetNextBuffer();

            uint8_t *stage_data = sb.buf->Map();

            const int img_data_size = 4 * img_data_w * img_data_h;
            if (!data.read((char *)stage_data, img_data_size)) {
                return false;
            }

            sb.buf->Unmap();

            Ren::Tex2DParams p;
            p.w = img_data_w;
            p.h = img_data_h;
            p.format = Ren::eTexFormat::RawRGBA8888;
            p.sampling.filter =
                draw_mode_ == eDrawMode::Passthrough ? Ren::eTexFilter::NoFilter : Ren::eTexFilter::BilinearNoMipmap;
            p.sampling.wrap = Ren::eTexWrap::ClampToBorder;

            Ren::eTexLoadStatus status;
            tex_ = ctx.LoadTextureRegion(name, *sb.buf, 0, img_data_size, sb.cmd_buf, p, &status);
        } else if (chunk_id == uint32_t(eFontFileChunk::FontChGlyphData)) {
            if (!data.read((char *)&glyph_range_count_, sizeof(uint32_t))) {
                return false;
            }

            glyph_ranges_ = std::make_unique<glyph_range_t[]>(glyph_range_count_);

            if (!data.read((char *)glyph_ranges_.get(), glyph_range_count_ * sizeof(glyph_range_t))) {
                return false;
            }

            glyphs_count_ = 0;
            for (uint32_t j = 0; j < glyph_range_count_; j++) {
                glyphs_count_ += (glyph_ranges_[j].end - glyph_ranges_[j].beg);
            }

            glyphs_ = std::make_unique<glyph_info_t[]>(glyphs_count_);

            if (!data.read((char *)glyphs_.get(), glyphs_count_ * sizeof(glyph_info_t))) {
                return false;
            }
        }

        data.seekg(old_pos, std::ios::beg);
    }

    return true;
}

float Gui::BitmapFont::GetWidth(std::string_view text, const float scale, const BaseElement *parent) const {
    int cur_x = 0;

    const glyph_range_t *glyph_ranges = glyph_ranges_.get();
    const glyph_info_t *glyphs = glyphs_.get();

    size_t text_len = text.size();

    int char_pos = 0;
    while (text_len--) {
        uint32_t unicode;
        char_pos += ConvChar_UTF8_to_Unicode(&text[char_pos], unicode);

        uint32_t glyph_index = 0;
        for (uint32_t i = 0; i < glyph_range_count_; i++) {
            const glyph_range_t &rng = glyph_ranges[i];

            if (unicode >= rng.beg && unicode < rng.end) {
                glyph_index += (unicode - rng.beg);
                break;
            } else {
                glyph_index += (rng.end - rng.beg);
            }
        }
        assert(glyph_index < glyphs_count_);

        const glyph_info_t &glyph = glyphs[glyph_index];
        cur_x += glyph.adv[0];
    }

    const float mul = scale * parent->size()[0] / float(parent->size_px()[0]);
    return float(cur_x) * mul;
}

float Gui::BitmapFont::DrawText(Renderer *r, std::string_view text, const Vec2f &pos, const uint8_t col[4],
                                const float scale, const BaseElement *parent) const {
    const glyph_range_t *glyph_ranges = glyph_ranges_.get();
    const glyph_info_t *glyphs = glyphs_.get();

    const Vec2f p = parent->pos() + 0.5f * (pos + Vec2f(1, 1)) * parent->size(),
                m = scale * parent->size() / (Vec2f)parent->size_px();

    const uint16_t uvs_offset[2] = {(uint16_t)tex_->pos(0), (uint16_t)tex_->pos(1)},
                   tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_->pos(2)));

    uint16_t u16_draw_mode = 0;
    if (draw_mode_ == eDrawMode::DistanceField) {
        u16_draw_mode = 32727;
    } else if (draw_mode_ == eDrawMode::BlitDistanceField) {
        u16_draw_mode = 65535;
    }

    vertex_t *vtx_data;
    int vtx_avail = 0;
    uint16_t *ndx_data;
    int ndx_avail = 0;
    int ndx_offset = r->AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    int cur_x = 0;

    const Vec2f uvs_scale = 1.0f / Vec2f{float(Ren::TextureAtlasWidth), float(Ren::TextureAtlasHeight)};

    const std::optional<Vec4f> clip = r->GetClipArea();

    int char_pos = 0;
    while (char_pos < text.size()) {
        uint32_t unicode;
        char_pos += ConvChar_UTF8_to_Unicode(&text[char_pos], unicode);

        uint32_t glyph_index = 0;
        for (uint32_t i = 0; i < glyph_range_count_; i++) {
            const glyph_range_t &rng = glyph_ranges[i];

            if (unicode >= rng.beg && unicode < rng.end) {
                glyph_index += (unicode - rng.beg);
                break;
            } else {
                glyph_index += (rng.end - rng.beg);
            }
        }
        assert(glyph_index < glyphs_count_);

        const glyph_info_t &glyph = glyphs[glyph_index];
        if (glyph.res[0]) {
            Vec4f pos_uvs[2] = {Vec4f{p[0] + float(cur_x + glyph.off[0] - 1) * m[0],
                                      p[1] + float(glyph.off[1] - 1) * m[1],
                                      uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] - 1),
                                      uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] + glyph.res[1] + 1)},
                                Vec4f{p[0] + float(cur_x + glyph.off[0] + glyph.res[0] + 1) * m[0],
                                      p[1] + float(glyph.off[1] + glyph.res[1] + 1) * m[1],
                                      uvs_scale[0] * float(uvs_offset[0] + glyph.pos[0] + glyph.res[0] + 1),
                                      uvs_scale[1] * float(uvs_offset[1] + glyph.pos[1] - 1)}};
            if (clip && !ClipQuadToArea(pos_uvs, *clip)) {
                cur_x += glyph.adv[0];
                continue;
            }

            vtx_avail -= 4;
            ndx_avail -= 6;
            assert(vtx_avail > 0 && ndx_avail > 0);

            cur_vtx->pos[0] = pos_uvs[0][0];
            cur_vtx->pos[1] = pos_uvs[0][1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
            cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = u16_draw_mode;
            ++cur_vtx;

            cur_vtx->pos[0] = pos_uvs[1][0];
            cur_vtx->pos[1] = pos_uvs[0][1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
            cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = u16_draw_mode;
            ++cur_vtx;

            cur_vtx->pos[0] = pos_uvs[1][0];
            cur_vtx->pos[1] = pos_uvs[1][1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
            cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = u16_draw_mode;
            ++cur_vtx;

            cur_vtx->pos[0] = pos_uvs[0][0];
            cur_vtx->pos[1] = pos_uvs[1][1];
            cur_vtx->pos[2] = 0.0f;
            memcpy(cur_vtx->col, col, 4);
            cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
            cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
            cur_vtx->uvs[2] = tex_layer;
            cur_vtx->uvs[3] = u16_draw_mode;
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

    r->SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));

    return float(cur_x) * m[0];
}

int Gui::BitmapFont::CheckText(std::string_view text, const Vec2f &pos, const Vec2f &press_pos, const float scale,
                               float &out_char_offset, const BaseElement *parent) const {
    const glyph_range_t *glyph_ranges = glyph_ranges_.get();
    const glyph_info_t *glyphs = glyphs_.get();

    const Vec2f p = parent->pos() + 0.5f * (pos + Vec2f(1, 1)) * parent->size(),
                m = scale * parent->size() / (Vec2f)parent->size_px();

    int cur_x = 0;
    int char_index = 0;

    int char_pos = 0;
    while (char_pos < text.size()) {
        uint32_t unicode;
        char_pos += ConvChar_UTF8_to_Unicode(&text[char_pos], unicode);

        uint32_t glyph_index = 0;
        for (uint32_t i = 0; i < glyph_range_count_; i++) {
            const glyph_range_t &rng = glyph_ranges[i];

            if (unicode >= rng.beg && unicode < rng.end) {
                glyph_index += (unicode - rng.beg);
                break;
            } else {
                glyph_index += (rng.end - rng.beg);
            }
        }
        assert(glyph_index < glyphs_count_);

        const glyph_info_t &glyph = glyphs[glyph_index];
        if (glyph.res[0]) {
            const float corners[2][2]{
                {p[0] + float(cur_x + glyph.off[0] - 1) * m[0], p[1] + float(glyph.off[1] - 1) * m[1]},
                {p[0] + float(cur_x + glyph.off[0] + glyph.res[0] + 1) * m[0],
                 p[1] + float(glyph.off[1] + glyph.res[1] + 1) * m[1]}};

            if (press_pos[0] >= corners[0][0] &&
                press_pos[0] <= corners[1][0] /*&&
                press_pos[1] >= corners[0][1] &&
                press_pos[1] <= corners[1][1]*/) {
                out_char_offset = float(cur_x) * m[0];
                return char_index;
            }
        }

        cur_x += glyph.adv[0];
        char_index++;
    }

    return -1;
}
