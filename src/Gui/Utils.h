#pragma once

#include <Ren/MVec.h>

namespace Gui {
Ren::Vec2f MapPointToScreen(const Ren::Vec2i &p, const Ren::Vec2i &res);

//
// SDF font generation
//

// Used for debugging
void DrawBezier1ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, int stride, int channels, uint8_t *out_rgba);
void DrawBezier2ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, int stride, int channels, uint8_t *out_rgba);
void DrawBezier3ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, const Ren::Vec2d &p3, int stride, int channels, uint8_t *out_rgba);

// Solve cubic equation (x ^ 3) + a * (x ^ 2) + b * x + c = 0
int SolveCubic(double a, double b, double c, double x[3]);

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

void PreprocessBezierShape(bezier_seg_t *segs, int count, const double max_soft_angle_rad);
dist_result_t BezierSegmentDistance(const bezier_seg_t &seg, const Ren::Vec2d &p);

int FixSDFCollisions(uint8_t *img_data, int w, int h, int channels, int threshold);
}

