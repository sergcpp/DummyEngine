#pragma once

#include <cstdint>
#include <utility>

#include <Ren/MVec.h>

namespace Gui {
Ren::Vec2f MapPointToScreen(const Ren::Vec2i &p, const Ren::Vec2i &res);

//
// Unicode stuff
//
const std::pair<uint32_t, uint32_t>
    g_unicode_latin_range           = { 0x0020, 0x007F },
    g_unicode_cyrilic_range_full    = { 0x0400, 0x04FF },
    g_unicode_cyrilic_range_min     = { 1040,   1104   };

const uint32_t g_unicode_umlauts[] = { 223, 228, 246, 252, 196, 214, 220 };
const uint32_t g_unicode_spacebar = 0x20;
const uint32_t g_unicode_heart = 0x2665;
const uint32_t g_unicode_less_than = 0x3C;
const uint32_t g_unicode_greater_than = 0x3E;
const uint32_t g_unicode_ampersand = 0x26;
const uint32_t g_unicode_semicolon = 0x3B;

int ConvChar_UTF8_to_Unicode(const char *utf8, uint32_t &out_unicode);
int ConvChar_UTF8_to_UTF16(const char *utf8, uint16_t out_utf16[2]);

//
// SDF font generation
//

// Used for debugging
void DrawBezier1ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, int stride, int channels, uint8_t *out_rgba);
void DrawBezier2ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, int stride, int channels, uint8_t *out_rgba);
void DrawBezier3ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, const Ren::Vec2d &p3, int stride, int channels, uint8_t *out_rgba);

struct dist_result_t {
    double sdist, pseudodist, ortho;
    double t, dot;
};

// Calc distance from point to curve
dist_result_t Bezier1Distance(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p);
dist_result_t Bezier2Distance(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, const Ren::Vec2d &p);
dist_result_t Bezier3Distance(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, const Ren::Vec2d &p3, const Ren::Vec2d &p);

struct bezier_seg_t {
    int order;
    bool is_closed, is_hard;
    Ren::Vec2d p0, p1;
    Ren::Vec2d c0, c1;
    Ren::Vec2d dAdt1, dBdt0;
};
static_assert(sizeof(bezier_seg_t) == 104, "!");

void PreprocessBezierShape(bezier_seg_t *segs, int count, double max_soft_angle_rad);
dist_result_t BezierSegmentDistance(const bezier_seg_t &seg, const Ren::Vec2d &p);

int FixSDFCollisions(uint8_t *img_data, int w, int h, int channels, int threshold);
}

