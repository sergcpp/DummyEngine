#include "BitmapFont.h"

#include <cassert>
#include <cstring>

#include <Sys/AssetFile.h>

#include "BaseElement.h"
#include "Renderer.h"
#include "Utils.h"

namespace BitmapFontConstants {

}

std::vector<float> Gui::BitmapFont::default_pos_buf, Gui::BitmapFont::default_uvs_buf;
std::vector<uint16_t> Gui::BitmapFont::default_indices_buf;

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

float Gui::BitmapFont::GetTriangles(const char *text, std::vector<float> &positions, std::vector<float> &uvs,
                                    std::vector<uint16_t> &indices, const Vec2f &pos, const BaseElement *parent) const {
    const Vec2f
        p = parent->pos() + 0.5f * (pos + Vec2f(1, 1)) * parent->size(),
        m = scale_ * parent->size() / (Vec2f)parent->size_px();

    positions.clear();
    uvs.clear();
    indices.clear();

    int cur_x = 0;

    const Vec2f uvs_scale = 1.0f / Vec2f{ (float)tex_res_[0], (float)tex_res_[1] };

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
            auto index_offset = uint16_t(4 * uvs.size() / 8);

            uvs.resize(uvs.size() + 8);
            positions.resize(positions.size() + 12);
            indices.resize(indices.size() + 6);

            auto *_uvs = (Vec2f *)(uvs.data() + uvs.size() - 8);

            _uvs[0] = uvs_scale * Vec2f{ float(glyph.pos[0] - 1),                   float(glyph.pos[1] + glyph.res[1] + 1) };
            _uvs[1] = uvs_scale * Vec2f{ float(glyph.pos[0] + glyph.res[0] + 1),    float(glyph.pos[1] + glyph.res[1] + 1) };
            _uvs[2] = uvs_scale * Vec2f{ float(glyph.pos[0] + glyph.res[0] + 1),    float(glyph.pos[1] - 1) };
            _uvs[3] = uvs_scale * Vec2f{ float(glyph.pos[0] - 1),                   float(glyph.pos[1] - 1) };
            
            auto *_pos = (Vec3f *)(positions.data() + positions.size() - 12);

            _pos[0] = Vec3f{ p[0] + float(cur_x + glyph.off[0] - 1) * m[0],                 p[1] + float(glyph.off[1] - 1) * m[1], 0.0f };
            _pos[1] = Vec3f{ p[0] + float(cur_x + glyph.off[0] + glyph.res[0] + 1) * m[0],  p[1] + float(glyph.off[1] - 1) * m[1], 0.0f };
            _pos[2] = Vec3f{ p[0] + float(cur_x + glyph.off[0] + glyph.res[0] + 1) * m[0],  p[1] + float(glyph.off[1] + glyph.res[1] + 1) * m[1], 0.0f };
            _pos[3] = Vec3f{ p[0] + float(cur_x + glyph.off[0] - 1) * m[0],                 p[1] + float(glyph.off[1] + glyph.res[1] + 1) * m[1], 0.0f };

            uint16_t *_indices = indices.data() + indices.size() - 6;

            _indices[0] = index_offset + 0;
            _indices[1] = index_offset + 1;
            _indices[2] = index_offset + 2;

            _indices[3] = index_offset + 0;
            _indices[4] = index_offset + 2;
            _indices[5] = index_offset + 3;
        }

        cur_x += glyph.adv[0];
    }

    return float(cur_x) * m[0];
}

void Gui::BitmapFont::DrawText(Renderer *r, const char *text, const Vec2f &pos, const BaseElement *parent) {
    GetTriangles(text, default_pos_buf, default_uvs_buf, default_indices_buf, pos, parent);
    if (default_pos_buf.empty()) {
        return;
    }

    const Renderer::DrawParams &cur = r->GetParams();

    r->EmplaceParams(Vec4f{ 1.0f, 1.0f, 1.0f, draw_mode_ == DrPassthrough ? 0.0f : 1.0f }, cur.z_val(), blend_mode_, cur.scissor_test());
    r->DrawUIElement(tex_, Gui::PrimTriangle, default_pos_buf, default_uvs_buf, default_indices_buf);
    r->PopParams();
}