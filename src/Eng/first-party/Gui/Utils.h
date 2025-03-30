#pragma once

#include <cstdint>
#include <utility>

#include "MVec.h"

namespace Gui {
Vec2f MapPointToScreen(const Vec2i &p, const Vec2i &res);
bool ClipQuadToArea(Vec4f pos[2], const Vec4f &clip);
int ClipPolyToArea(Vec4f *vertices, int vertex_count, const Vec4f &clip);

//
// Unicode stuff
//
const std::pair<uint32_t, uint32_t> g_unicode_latin_range = {0x0020, 0x007F},
                                    g_unicode_cyrilic_range_full = {0x0400, 0x04FF},
                                    g_unicode_cyrilic_range_min = {1040, 1104};

const uint32_t g_unicode_umlauts[] = {223, 228, 246, 252, 196, 214, 220};
const uint32_t g_unicode_spacebar = 0x20;
const uint32_t g_unicode_heart = 0x2665;
const uint32_t g_unicode_less_than = 0x3C;
const uint32_t g_unicode_greater_than = 0x3E;
const uint32_t g_unicode_ampersand = 0x26;
const uint32_t g_unicode_semicolon = 0x3B;
// TODO: incomplete!
const uint32_t g_unicode_punctuation[] = {0x0021, 0x0022, 0x0023, 0x0025, 0x0026, 0x0027, 0x002A, 0x002C, 0x002E,
                                          0x002F, 0x003A, 0x003B, 0x003F, 0x0040, 0x005C, 0x00A1, 0x00A7};

int ConvChar_UTF8_to_Unicode(const char *utf8, uint32_t &out_unicode);
int ConvChar_UTF8_to_UTF16(const char *utf8, uint16_t out_utf16[2]);

int ConvChar_Unicode_to_UTF8(uint32_t unicode, char *out_utf8);

int CalcUTF8Length(const char *utf8);

//
// SDF font generation
//

// Used for debugging
void DrawBezier1ToBitmap(const Vec2d &p0, const Vec2d &p1, int stride, int channels, uint8_t *out_rgba);
void DrawBezier2ToBitmap(const Vec2d &p0, const Vec2d &p1, const Vec2d &p2, int stride, int channels,
                         uint8_t *out_rgba);
void DrawBezier3ToBitmap(const Vec2d &p0, const Vec2d &p1, const Vec2d &p2, const Vec2d &p3, int stride, int channels,
                         uint8_t *out_rgba);

struct dist_result_t {
    double sdist, pseudodist, ortho;
    double t, dot;
};

// Calc distance from point to curve
dist_result_t Bezier1Distance(const Vec2d &p0, const Vec2d &p1, const Vec2d &p);
dist_result_t Bezier2Distance(const Vec2d &p0, const Vec2d &p1, const Vec2d &p2, const Vec2d &p);
dist_result_t Bezier3Distance(const Vec2d &p0, const Vec2d &p1, const Vec2d &p2, const Vec2d &p3, const Vec2d &p);

struct bezier_seg_t {
    int order;
    bool is_closed, is_hard;
    Vec2d p0, p1;
    Vec2d c0, c1;
    Vec2d dAdt1, dBdt0;
};
static_assert(sizeof(bezier_seg_t) == 104);

void PreprocessBezierShape(bezier_seg_t *segs, int count, double max_soft_angle_rad);
dist_result_t BezierSegmentDistance(const bezier_seg_t &seg, const Vec2d &p);

int FixSDFCollisions(uint8_t *img_data, int w, int h, int channels, int threshold);
} // namespace Gui
