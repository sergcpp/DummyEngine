#include "SceneManager.h"

#include <fstream>
#include <numeric>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../third-party/stb/stb_truetype.h"

#include <Ray/internal/TextureSplitter.h>

#include "../Gui/BitmapFont.h"
#include "../Gui/Utils.h"

bool Eng::SceneManager::HConvTTFToFont(assets_context_t &ctx, const char *in_file, const char *out_file,
                                       Ren::SmallVectorImpl<std::string> &) {
    using namespace Ren;

    ctx.log->Info("[PrepareAssets] Conv %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    if (!src_stream) {
        return false;
    }
    auto src_size = (size_t)src_stream.tellg();
    src_stream.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_stream.read((char *)&src_buf[0], src_size);

    stbtt_fontinfo font;
    int res = stbtt_InitFont(&font, &src_buf[0], 0);
    if (!res) {
        ctx.log->Error("stbtt_InitFont failed (%s)", in_file);
        return false;
    }

    const bool is_sdf_font = strstr(in_file, "_sdf") != nullptr, is_inv_wind = strstr(in_file, "_inv") != nullptr;
    float line_height = 48.0f;

    int temp_bitmap_res[2] = {512, 256};

    if (strstr(in_file, "_9px")) {
        line_height = 9.0f;
    } else if (strstr(in_file, "_12px")) {
        line_height = 12.0f;
        // temp_bitmap_res[0] = 256;
        // temp_bitmap_res[1] = 128;
    } else if (strstr(in_file, "_16px")) {
        line_height = 16.0f;
        // temp_bitmap_res[0] = 256;
        // temp_bitmap_res[1] = 128;
    } else if (strstr(in_file, "_24px")) {
        line_height = 24.0f;
    } else if (strstr(in_file, "_32px")) {
        line_height = 32.0f;
    } else if (strstr(in_file, "_36px")) {
        line_height = 36.0f;
    } else if (strstr(in_file, "_48px")) {
        line_height = 48.0f;
        temp_bitmap_res[0] = 512;
        temp_bitmap_res[1] = 512;
    }

    const float scale = stbtt_ScaleForPixelHeight(&font, line_height);

    const int sdf_radius_px = is_sdf_font ? 1 : 0;
    const int padding = 1;

    const Gui::glyph_range_t glyph_ranges[] = {
        {Gui::g_unicode_latin_range.first, Gui::g_unicode_latin_range.second},
        {Gui::g_unicode_cyrilic_range_min.first, Gui::g_unicode_cyrilic_range_min.second},
        {Gui::g_unicode_umlauts[0], Gui::g_unicode_umlauts[0] + 1},
        {Gui::g_unicode_umlauts[1], Gui::g_unicode_umlauts[1] + 1},
        {Gui::g_unicode_umlauts[2], Gui::g_unicode_umlauts[2] + 1},
        {Gui::g_unicode_umlauts[3], Gui::g_unicode_umlauts[3] + 1},
        {Gui::g_unicode_umlauts[4], Gui::g_unicode_umlauts[4] + 1},
        {Gui::g_unicode_umlauts[5], Gui::g_unicode_umlauts[5] + 1},
        {Gui::g_unicode_umlauts[6], Gui::g_unicode_umlauts[6] + 1},
        {Gui::g_unicode_ampersand, Gui::g_unicode_ampersand + 1},
        {Gui::g_unicode_semicolon, Gui::g_unicode_semicolon + 1},
        {Gui::g_unicode_heart, Gui::g_unicode_heart + 1}};
    const int glyph_range_count = sizeof(glyph_ranges) / sizeof(glyph_ranges[0]);

    const int total_glyph_count =
        std::accumulate(std::begin(glyph_ranges), std::end(glyph_ranges), 0,
                        [](int sum, const Gui::glyph_range_t val) -> int { return sum + int(val.end - val.beg); });

    std::unique_ptr<Gui::glyph_info_t[]> out_glyphs(new Gui::glyph_info_t[total_glyph_count]);
    int out_glyph_count = 0;

    std::unique_ptr<uint8_t[]> temp_bitmap(new uint8_t[temp_bitmap_res[0] * temp_bitmap_res[1] * 4]);
    Ray::TextureSplitter temp_bitmap_splitter(temp_bitmap_res);

    std::fill(&temp_bitmap[0], &temp_bitmap[0] + 4 * temp_bitmap_res[0] * temp_bitmap_res[1], 0);

    for (const Gui::glyph_range_t &range : glyph_ranges) {
        ctx.log->Info("Processing glyph range (%i - %i)", range.beg, range.end);
        for (uint32_t i = range.beg; i < range.end; i++) {
            const int glyph_index = stbtt_FindGlyphIndex(&font, i);

            int x0, y0, x1, y1;
            const bool is_drawable = stbtt_GetGlyphBox(&font, glyph_index, &x0, &y0, &x1, &y1) != 0;

            int advance_width, left_side_bearing;
            stbtt_GetGlyphHMetrics(&font, glyph_index, &advance_width, &left_side_bearing);

            int glyph_pos[2] = {}, glyph_res[2] = {}, glyph_res_act[2] = {};
            if (is_drawable) {
                glyph_res_act[0] = int(std::round(scale * float(x1 - x0 + 1))) + 2 * sdf_radius_px;
                glyph_res_act[1] = int(std::round(scale * float(y1 - y0 + 1))) + 2 * sdf_radius_px;

                glyph_res[0] = glyph_res_act[0] + 2 * padding;
                glyph_res[1] = glyph_res_act[1] + 2 * padding;

                const int node_index = temp_bitmap_splitter.Allocate(glyph_res, glyph_pos);
                if (node_index == -1) {
                    throw std::runtime_error("Region allocation failed!");
                }
            }

            Gui::glyph_info_t &out_glyph = out_glyphs[out_glyph_count++];

            out_glyph.pos[0] = glyph_pos[0] + padding;
            out_glyph.pos[1] = glyph_pos[1] + padding;
            out_glyph.res[0] = glyph_res_act[0];
            out_glyph.res[1] = glyph_res_act[1];
            out_glyph.off[0] = int(std::round(scale * float(x0)));
            out_glyph.off[1] = int(std::round(scale * float(y0)));
            out_glyph.adv[0] = int(std::round(scale * float(advance_width)));
            out_glyph.adv[1] = 0;

            if (!is_drawable)
                continue;

            using bezier_shape = std::vector<Gui::bezier_seg_t>;
            std::vector<bezier_shape> shapes;

            { // Get glyph shapes
                stbtt_vertex *vertices = nullptr;
                const int vertex_count = stbtt_GetGlyphShape(&font, glyph_index, &vertices);

                { // transform input data
                    const auto pos_offset = Vec2d{double(padding + sdf_radius_px), double(padding + sdf_radius_px)};

                    Vec2i cur_p;

                    for (int j = 0; j < vertex_count; j++) {
                        const stbtt_vertex &v = vertices[j];

                        const Vec2d p0 = pos_offset + Vec2d{cur_p - Vec2i{x0, y0}} * scale,
                                    c0 = pos_offset + Vec2d{double(v.cx - x0), double(v.cy - y0)} * scale,
                                    c1 = pos_offset + Vec2d{double(v.cx1 - x0), double(v.cy1 - y0)} * scale,
                                    p1 = pos_offset + Vec2d{double(v.x - x0), double(v.y - y0)} * scale;

                        if (v.type == STBTT_vmove) {
                            if (shapes.empty() || !shapes.back().empty()) {
                                // start new shape
                                shapes.emplace_back();
                            }
                        } else {
                            // 1 - line; 2,3 - bezier with 1 and 2 control points
                            const int order = (v.type == STBTT_vline) ? 1 : ((v.type == STBTT_vcurve) ? 2 : 3);

                            shapes.back().push_back(
                                {order, false /* is_closed */, false /* is_hard */, p0, p1, c0, c1});
                        }

                        cur_p = Vec2i{v.x, v.y};
                    }
                }

                stbtt_FreeShape(&font, vertices);
            }

            if (!is_sdf_font) {
                //
                // Simple rasterization
                //
                const int samples = 4;

                // Loop through image pixels
                for (int y = 0; y < glyph_res[1]; y++) {
                    for (int x = 0; x < glyph_res[0]; x++) {
                        uint32_t out_val = 0;

                        for (int dy = 0; dy < samples; dy++) {
                            for (int dx = 0; dx < samples; dx++) {
                                const auto p = Vec2d{double(x) + (0.5 + double(dx)) / samples,
                                                     double(y) + (0.5 + double(dy)) / samples};

                                double min_sdist = std::numeric_limits<double>::max(),
                                       min_dot = std::numeric_limits<double>::lowest();

                                for (const bezier_shape &sh : shapes) {
                                    for (const Gui::bezier_seg_t &seg : sh) {
                                        const Gui::dist_result_t result = Gui::BezierSegmentDistance(seg, p);

                                        if (std::abs(result.sdist) < std::abs(min_sdist) ||
                                            (std::abs(result.sdist) == std::abs(min_sdist) && result.dot < min_dot)) {
                                            min_sdist = result.sdist;
                                            min_dot = result.dot;
                                        }
                                    }
                                }

                                out_val += (min_sdist > 0.0) ? 255 : 0;
                            }
                        }

                        // Write output value
                        const int out_x = glyph_pos[0] + x, out_y = glyph_pos[1] + (glyph_res[1] - y - 1);
                        uint8_t *out_pixel = &temp_bitmap[4 * (out_y * temp_bitmap_res[0] + out_x)];

                        out_pixel[0] = out_pixel[1] = out_pixel[2] = 255;
                        out_pixel[3] = (out_val / (samples * samples));
                    }
                }
            } else {
                //
                // Multi-channel SDF font
                //

                // find hard edges, mark if closed etc.
                for (bezier_shape &sh : shapes) {
                    Gui::PreprocessBezierShape(sh.data(), (int)sh.size(),
                                               30.0 * Ren::Pi<double>() / 180.0 /* angle threshold */);
                }

                // Loop through image pixels
                for (int y = 0; y < glyph_res[1]; y++) {
                    for (int x = 0; x < glyph_res[0]; x++) {
                        const auto p = Vec2d{double(x) + 0.5, double(y) + 0.5};

                        // Per channel distances (used for multi-channel sdf)
                        Gui::dist_result_t min_result[3];
                        for (Gui::dist_result_t &r : min_result) {
                            r.sdist = std::numeric_limits<double>::max();
                            r.ortho = r.dot = std::numeric_limits<double>::lowest();
                        }

                        // Simple distances (used for normal sdf)
                        double min_sdf_sdist = std::numeric_limits<double>::max(),
                               min_sdf_dot = std::numeric_limits<double>::lowest();

                        for (const bezier_shape &sh : shapes) {
                            int edge_color_index = 0;
                            static const Vec3i edge_colors[] = {Vec3i{255, 0, 255}, Vec3i{255, 255, 0},
                                                                Vec3i{0, 255, 255}};

                            for (int i = 0; i < int(sh.size()); i++) {
                                const Gui::bezier_seg_t &seg = sh[i];
                                const Gui::dist_result_t result = Gui::BezierSegmentDistance(seg, p);

                                if (i != 0 && seg.is_hard) {
                                    if ((i == sh.size() - 1) && sh[0].is_closed && !sh[0].is_hard) {
                                        edge_color_index = 0;
                                    } else {
                                        if (edge_color_index == 1) {
                                            edge_color_index = 2;
                                        } else {
                                            edge_color_index = 1;
                                        }
                                    }
                                }
                                const Vec3i &edge_color = edge_colors[edge_color_index];

                                for (int j = 0; j < 3; j++) {
                                    if (edge_color[j]) {
                                        if (std::abs(result.sdist) < std::abs(min_result[j].sdist) ||
                                            (std::abs(result.sdist) == std::abs(min_result[j].sdist) &&
                                             result.dot < min_result[j].dot)) {
                                            min_result[j] = result;
                                        }
                                    }
                                }

                                if (std::abs(result.sdist) < std::abs(min_sdf_sdist) ||
                                    (std::abs(result.sdist) == std::abs(min_sdf_sdist) && result.dot < min_sdf_dot)) {
                                    min_sdf_sdist = result.sdist;
                                    min_sdf_dot = result.dot;
                                }
                            }
                        }

                        // Write distance to closest shape
                        const int out_x = glyph_pos[0] + x, out_y = glyph_pos[1] + (glyph_res[1] - y - 1);
                        uint8_t *out_pixel = &temp_bitmap[4 * (out_y * temp_bitmap_res[0] + out_x)];

                        for (int j = 0; j < 3; j++) {
                            uint8_t out_val = 0;
                            if (min_result[j].sdist != std::numeric_limits<double>::max()) {
                                min_result[j].pseudodist =
                                    Clamp(0.5 + (min_result[j].pseudodist / (2.0 * sdf_radius_px)), 0.0, 1.0);
                                out_val = (uint8_t)std::max(std::min(int(255 * min_result[j].pseudodist), 255), 0);
                            }
                            out_pixel[j] = out_val;
                        }

                        min_sdf_sdist = Clamp(0.5 + (min_sdf_sdist / (2.0 * sdf_radius_px)), 0.0, 1.0);
                        out_pixel[3] = (uint8_t)std::max(std::min(int(255 * min_sdf_sdist), 255), 0);
                    }
                }
            }
        }
    }

    if (is_sdf_font) {
        // Fix collisions of uncorrelated areas
        Gui::FixSDFCollisions(temp_bitmap.get(), temp_bitmap_res[0], temp_bitmap_res[1], 4, 200 /* threshold */);
    }

    if (is_inv_wind) {
        // Flip colors (font has inversed winding order)
        for (int y = 0; y < temp_bitmap_res[1]; y++) {
            for (int x = 0; x < temp_bitmap_res[0]; x++) {
                uint8_t *out_pixel = &temp_bitmap[4 * (y * temp_bitmap_res[0] + x)];

                if (is_sdf_font) {
                    out_pixel[0] = 255 - out_pixel[0];
                    out_pixel[1] = 255 - out_pixel[1];
                    out_pixel[2] = 255 - out_pixel[2];
                }
                out_pixel[3] = 255 - out_pixel[3];
            }
        }
    }

    assert(out_glyph_count == total_glyph_count);

    /*if (strstr(in_file, "Roboto-Regular_16px")) {
        WriteImage(temp_bitmap.get(), temp_bitmap_res[0], temp_bitmap_res[1], 4, false,
    "test.png");
    }*/

    std::ofstream out_stream(out_file, std::ios::binary);
    const uint32_t header_size =
        4 + sizeof(uint32_t) + uint32_t(Gui::eFontFileChunk::FontChCount) * 3 * sizeof(uint32_t);
    uint32_t hdr_offset = 0, data_offset = header_size;

    { // File format string
        const char signature[] = {'F', 'O', 'N', 'T'};
        out_stream.write(signature, 4);
        hdr_offset += 4;
    }

    { // Header size
        out_stream.write((const char *)&header_size, sizeof(uint32_t));
        hdr_offset += sizeof(uint32_t);
    }

    { // Typograph data offsets
        const uint32_t typo_data_chunk_id = uint32_t(Gui::eFontFileChunk::FontChTypoData),
                       typo_data_offset = data_offset, typo_data_size = sizeof(Gui::typgraph_info_t);
        out_stream.write((const char *)&typo_data_chunk_id, sizeof(uint32_t));
        out_stream.write((const char *)&typo_data_offset, sizeof(uint32_t));
        out_stream.write((const char *)&typo_data_size, sizeof(uint32_t));
        hdr_offset += 3 * sizeof(uint32_t);
        data_offset += typo_data_size;
    }

    { // Image data offsets
        const uint32_t img_data_chunk_id = uint32_t(Gui::eFontFileChunk::FontChImageData),
                       img_data_offset = data_offset,
                       img_data_size =
                           2 * sizeof(uint16_t) + 2 * sizeof(uint16_t) + 4 * temp_bitmap_res[0] * temp_bitmap_res[1];
        out_stream.write((const char *)&img_data_chunk_id, sizeof(uint32_t));
        out_stream.write((const char *)&img_data_offset, sizeof(uint32_t));
        out_stream.write((const char *)&img_data_size, sizeof(uint32_t));
        hdr_offset += 3 * sizeof(uint32_t);
        data_offset += img_data_size;
    }

    { // Glyph data offsets
        const uint32_t glyph_data_chunk_id = uint32_t(Gui::eFontFileChunk::FontChGlyphData),
                       glyph_data_offset = data_offset,
                       glyph_data_size =
                           sizeof(uint32_t) + sizeof(glyph_ranges) + total_glyph_count * sizeof(Gui::glyph_info_t);
        out_stream.write((const char *)&glyph_data_chunk_id, sizeof(uint32_t));
        out_stream.write((const char *)&glyph_data_offset, sizeof(uint32_t));
        out_stream.write((const char *)&glyph_data_size, sizeof(uint32_t));
        hdr_offset += 3 * sizeof(uint32_t);
        data_offset += glyph_data_size;
    }

    assert(hdr_offset == header_size);

    { // Typograph data
        Gui::typgraph_info_t info = {};
        info.line_height = (uint32_t)line_height;

        out_stream.write((const char *)&info, sizeof(Gui::typgraph_info_t));
    }

    { // Image data
        const auto img_data_w = (uint16_t)temp_bitmap_res[0], img_data_h = (uint16_t)temp_bitmap_res[1];
        out_stream.write((const char *)&img_data_w, sizeof(uint16_t));
        out_stream.write((const char *)&img_data_h, sizeof(uint16_t));

        const uint16_t draw_mode = is_sdf_font ? uint16_t(Gui::eDrawMode::DistanceField)
                                               : uint16_t(Gui::eDrawMode::Passthrough),
                       blend_mode = uint16_t(Gui::eBlendMode::Alpha);
        out_stream.write((const char *)&draw_mode, sizeof(uint16_t));
        out_stream.write((const char *)&blend_mode, sizeof(uint16_t));

        out_stream.write((const char *)temp_bitmap.get(), 4 * temp_bitmap_res[0] * temp_bitmap_res[1]);
    }

    { // Glyph data
        const uint32_t u32_glyph_range_count = glyph_range_count;
        out_stream.write((const char *)&u32_glyph_range_count, sizeof(uint32_t));
        out_stream.write((const char *)&glyph_ranges[0].beg, sizeof(glyph_ranges));
        out_stream.write((const char *)out_glyphs.get(), total_glyph_count * sizeof(Gui::glyph_info_t));
    }

    return true;
}