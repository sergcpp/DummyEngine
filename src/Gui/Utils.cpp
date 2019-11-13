#include "Utils.h"

#define _abs(x) (((x) < 0.0) ? -(x) : (x))

namespace GuiInternal {
static double _root3(double x) {
    double s = 1.0;
    while (x < 1.0) {
        x *= 8.0;
        s *= 0.5;
    }
    while (x > 8.0) {
        x *= 0.125;
        s *= 2.0;
    }
    double r = 1.5;
    r -= 1.0 / 3.0 * (r - x / (r * r));
    r -= 1.0 / 3.0 * (r - x / (r * r));
    r -= 1.0 / 3.0 * (r - x / (r * r));
    r -= 1.0 / 3.0 * (r - x / (r * r));
    r -= 1.0 / 3.0 * (r - x / (r * r));
    r -= 1.0 / 3.0 * (r - x / (r * r));
    return r * s;
}

static double root3(double x) {
    if (x > 0.0) {
        return _root3(x);
    } else {
        if (x < 0.0) {
            return -_root3(-x);
        } else {
            return 0.0;
        }
    }
}

// Solve cubic equation (x ^ 3) + a * (x ^ 2) + b * x + c = 0
static int solve_cubic(double a, double b, double c, double x[3]) {
    using namespace GuiInternal;

    const double eps = std::numeric_limits<double>::epsilon();
    const double TwoPi = 2.0 * Ren::Pi<double>();

    const double a2 = a * a;
    double q = (a2 - 3.0 * b) / 9.0;
    const double r = (a * (2.0 * a2 - 9.0 * b) + 27.0 * c) / 54.0;
    // equation x^3 + q*x + r = 0
    const double r2 = r * r;
    const double q3 = q * q * q;
    double A, B;
    if (r2 <= (q3 + eps)) {
        double t = r / std::sqrt(q3);
        if (t < -1.0) t = -1.0;
        if (t > 1.0) t = 1.0;
        t = std::acos(t);
        a /= 3.0; q = -2.0 * std::sqrt(q);
        x[0] = q * std::cos(t / 3.0) - a;
        x[1] = q * std::cos((t + TwoPi) / 3.0) - a;
        x[2] = q * std::cos((t - TwoPi) / 3.0) - a;
        return 3;
    } else {
        A = -root3(_abs(r) + std::sqrt(r2 - q3));
        if (r < 0) {
            A = -A;
        }
        B = (A == 0.0 ? 0.0 : B = q / A);

        a /= 3.0;
        x[0] = (A + B) - a;
        x[1] = -0.5 * (A + B) - a;
        x[2] = 0.5 * std::sqrt(3.0) * (A - B);
        if (_abs(x[2]) < eps) { x[2] = x[1]; return 2; }
        return 1;
    }
}

static double cross2(const Ren::Vec2d &v1, const Ren::Vec2d &v2) {
    return v1[0] * v2[1] - v1[1] * v2[0];
}

static uint8_t median(uint8_t r, uint8_t g, uint8_t b) {
    return std::max(std::min(r, g), std::min(std::max(r, g), b));
}

}

Ren::Vec2f Gui::MapPointToScreen(const Vec2i &p, const Vec2i &res) {
    return (2.0f * Vec2f((float)p[0], (float)res[1] - p[1])) / (Vec2f)res + Vec2f(-1, -1);
}

int Gui::ConvChar_UTF8_to_Unicode(const char *utf8, uint32_t &out_unicode) {
    int i = 0;
    int todo;

    const auto ch0 = (uint8_t)utf8[i++];
    if (ch0 <= 0x7F) {
        out_unicode = ch0;
        todo = 0;
    } else if (ch0 <= 0xBF) {
        return 0;
    } else if (ch0 <= 0xDF) {
        out_unicode = ch0 & 0x1Fu;
        todo = 1;
    } else if (ch0 <= 0xEF) {
        out_unicode = ch0 & 0x0Fu;
        todo = 2;
    } else if (ch0 <= 0xF7) {
        out_unicode = ch0 & 0x07u;
        todo = 3;
    } else {
        return 0;
    }

    for (size_t j = 0; j < todo; ++j) {
        const auto ch1 = (uint8_t)utf8[i++];
        if (ch1 < 0x80 || ch1 > 0xBF) {
            return 0;
        }
        out_unicode <<= 6u;
        out_unicode += ch1 & 0x3Fu;
    }

    if ((out_unicode >= 0xD800 && out_unicode <= 0xDFFF) || out_unicode > 0x10FFFF) {
        return 0;
    }

    return 1 + todo;
}

int Gui::ConvChar_UTF8_to_UTF16(const char *utf8, uint16_t out_utf16[2]) {
    uint32_t uni;
    int consumed_bytes = ConvChar_UTF8_to_Unicode(utf8, uni);

    if (uni <= 0xFFFF) {
        out_utf16[0] = (uint16_t)uni;
        out_utf16[1] = 0;
    } else {
        // not a single-char utf16
        uni -= 0x10000;
        out_utf16[0] = (uint16_t)((uni >> 10u) + 0xD800);
        out_utf16[1] = (uint16_t)((uni & 0x3FFu) + 0xDC00);
    }

    return consumed_bytes;
}

void Gui::DrawBezier1ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, int stride, int channels, uint8_t *out_rgba) {
    auto p0i = Ren::Vec2i{ p0 }, p1i = Ren::Vec2i{ p1 };

    const int dx = _abs(p1i[0] - p0i[0]), sx = p0i[0] < p1i[0] ? 1 : -1;
    const int dy = _abs(p1i[1] - p0i[1]), sy = p0i[1] < p1i[1] ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    for (;;) {
        if (channels == 4) {
            out_rgba[4 * (p0i[1] * stride + p0i[0]) + 0] = 0xff;
            out_rgba[4 * (p0i[1] * stride + p0i[0]) + 1] = 0xff;
            out_rgba[4 * (p0i[1] * stride + p0i[0]) + 2] = 0xff;
            out_rgba[4 * (p0i[1] * stride + p0i[0]) + 3] = 0xff;
        } else if (channels == 3) {
            out_rgba[3 * (p0i[1] * stride + p0i[0]) + 0] = 0xff;
            out_rgba[3 * (p0i[1] * stride + p0i[0]) + 1] = 0xff;
            out_rgba[3 * (p0i[1] * stride + p0i[0]) + 2] = 0xff;
        }

        if (p0i[0] == p1i[0] && p0i[1] == p1i[1]) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; p0i[0] += sx; }
        if (e2 < dy) { err += dx; p0i[1] += sy; }
    }
}

void Gui::DrawBezier2ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, int stride, int channels, uint8_t *out_rgba) {
    const Ren::Vec2d
        pmin = Min(p0, Min(p1, p2)),
        pmax = Max(p0, Max(p1, p2));

    if ((pmax[0] - pmin[0]) < 4 && (pmax[1] - pmin[1]) < 4) {
        DrawBezier1ToBitmap(p0, p2, stride, channels, out_rgba);
    } else {
        const Ren::Vec2d
            p01 = (p0 + p1) / 2.0,
            p12 = (p1 + p2) / 2.0;
        const Ren::Vec2d p012 = (p01 + p12) / 2.0;

        DrawBezier2ToBitmap(p0, p01, p012, stride, channels, out_rgba);
        DrawBezier2ToBitmap(p012, p12, p2, stride, channels, out_rgba);
    }
}

void Gui::DrawBezier3ToBitmap(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, const Ren::Vec2d &p3, int stride, int channels, uint8_t *out_rgba) {
    const Ren::Vec2d
        pmin = Min(p0, Min(p1, p2)),
        pmax = Max(p0, Max(p1, p2));

    if ((pmax[0] - pmin[0]) < 4 && (pmax[1] - pmin[1]) < 4) {
        DrawBezier1ToBitmap(p0, p3, stride, channels, out_rgba);
    } else {
        const Ren::Vec2d
            p01 = (p0 + p1) / 2.0,
            p12 = (p1 + p2) / 2.0,
            p23 = (p2 + p3) / 2.0;

        const Ren::Vec2d
            p012 = (p01 + p12) / 2.0,
            p123 = (p12 + p23) / 2.0;

        const Ren::Vec2d p0123 = (p012 + p123) / 2.0;

        DrawBezier3ToBitmap(p0, p01, p012, p0123, stride, channels, out_rgba);
        DrawBezier3ToBitmap(p0123, p123, p23, p3, stride, channels, out_rgba);
    }
}

Gui::dist_result_t Gui::Bezier1Distance(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p) {
    using namespace GuiInternal;
    using namespace Ren;

    const Vec2d
        pp0 = p - p0,
        p10 = p1 - p0;
    const double t_unclumped = Dot(pp0, p10) / Dot(p10, p10);
    const double t = Clamp(t_unclumped, 0.0, 1.0);

    //const Vec2d to_closest = (t_unclumped < 0.5) ? p0 : p1;
    //const double to_closest_dist = Length(to_closest);

    if (t_unclumped > 0.0 && t_unclumped < 1.0) {
        /*const double p10_len = Length(p10);
        const Vec2d p10_ortho = { p10[1] / p10_len, -p10[0] / p10_len };

        const double ortho_dist = Dot(p10_ortho, pp0);
        if (ortho_dist < to_closest_dist) {
            return { ortho_dist, sign * Distance(_pp1, p), _abs(orthogonality), t, 0.0 };
        }*/

        const Vec2d
            _pp0 = p0 + t * p10,
            _pp1 = p0 + t_unclumped * p10;
        const double sign = cross2(p10, _pp0 - p) >= 0.0 ? 1.0 : -1.0;
        const double orthogonality = cross2(Normalize(p10), Normalize(p - _pp1));
        return { sign * Distance(_pp0, p), sign * Distance(_pp1, p), _abs(orthogonality), t, 0.0 };
    } else {
        const Vec2d
            _pp0 = (t_unclumped < 0.5) ? p0 : p1,
            _pp1 = p0 + t_unclumped * p10;
        const double sign = cross2(p10, _pp0 - p) >= 0.0 ? 1.0 : -1.0;
        const double orthogonality = cross2(Normalize(p10), Normalize(p - _pp1));
        return { sign * Distance(_pp0, p), sign * Distance(_pp1, p), _abs(orthogonality), t, _abs(Dot(Normalize(p10), Normalize(_pp0 - p))) };
    }
}

Gui::dist_result_t Gui::Bezier2Distance(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, const Ren::Vec2d &p) {
    using namespace GuiInternal;
    using namespace Ren;

    const Vec2d
        _p = p - p0,
        _p1 = p1 - p0,
        _p2 = p0 - 2 * p1 + p2;

    const double
        a = Dot(_p2, _p2),
        b = 3.0 * Dot(_p1, _p2) / a,
        c = (2.0 * Dot(_p1, _p1) - Dot(_p, _p2)) / a,
        d = -Dot(_p, _p1) / a;

    double roots[5] = { 0.0, 1.0 };
    const int count = solve_cubic(b, c, d, &roots[2]);
    assert(count);

    double min_sdist = std::numeric_limits<double>::max();
    double res_pseudodist, res_orthogonality, res_t;

    for (int i = 0; i < 2 + count; i++) {
        if (roots[i] < 0.0 || roots[i] > 1.0) continue;

        const double t1 = roots[i];

        const Vec2d pp1 = t1 * t1 * _p2 + 2.0 * t1 * _p1 + p0;
        const Vec2d derivative = 2.0 * t1 * _p2 + 2.0 * _p1;
        const double sign = cross2(derivative, pp1 - p) >= 0.0 ? 1.0 : -1.0;
        const double dist = sign * Distance(pp1, p);
        const double orthogonality = cross2(Normalize(derivative), Normalize(p - pp1));
        if (_abs(dist) < _abs(min_sdist)) {
            min_sdist = dist;
            res_pseudodist = dist;
            res_orthogonality = orthogonality;
            res_t = t1;
        }
    }

#if 1
    double res_dot = 0.0;

    if (res_t == 0.0) {   // Extend start of curve
        const Vec2d der0 = p0 - p1;
        const dist_result_t res0 = Bezier1Distance(p0 + der0 * 10000.0, p0, p);
        if (res0.t <= 1.0 /*&& _abs(res0.sdist) < _abs(min_sdist)*/) {
            //min_sdist = res0.sdist;
            res_pseudodist = res0.pseudodist;
            res_orthogonality = res0.ortho;
            //res_t = res0.t;
        }

        res_dot = Dot(Normalize(_p1), Normalize(-_p));
    } else if (res_t == 1.0) {   // Extend end of curve
        const Vec2d der1 = p2 - p1;
        const dist_result_t res1 = Bezier1Distance(p2, p2 + der1 * 10000.0, p);
        if (res1.t >= 0.0 /*&& _abs(res1.sdist) < _abs(min_sdist)*/) {
            //min_sdist = res1.sdist;
            res_pseudodist = res1.pseudodist;
            res_orthogonality = res1.ortho;
            //res_t = res1.t;
        }

        res_dot = Dot(Normalize(p2 - p1), Normalize(p2 - p));
    }
#endif

    return { min_sdist, res_pseudodist, _abs(res_orthogonality), res_t, _abs(res_dot) };
}

Gui::dist_result_t Gui::Bezier3Distance(const Ren::Vec2d &p0, const Ren::Vec2d &p1, const Ren::Vec2d &p2, const Ren::Vec2d &p3, const Ren::Vec2d &p) {
    assert(false && "Not implemented!");
    return {};
}

void Gui::PreprocessBezierShape(bezier_seg_t *segs, int count, const double max_soft_angle_rad) {
    using namespace Ren;
    if (count == 1) return;

    if (Distance(segs[0].p0, segs[count - 1].p1) < std::numeric_limits<double>::epsilon()) {
        segs[0].is_closed = true;
    }

    const double max_soft_dot = std::cos(max_soft_angle_rad);

    for (int i = 0; i < count; i++) {
        Gui::bezier_seg_t &seg = segs[i];
        assert(seg.order >= 1 && seg.order <= 3);

        Vec2d edge_derivatives[2];
        if (seg.order == 1) {
            edge_derivatives[0] = edge_derivatives[1] = seg.p1 - seg.p0;
        } else if (seg.order == 2) {
            edge_derivatives[0] = seg.c0 - seg.p0;
            edge_derivatives[1] = seg.p0 - seg.c1;
        } else /*if (seg.order == 3)*/ {
            edge_derivatives[0] = seg.c0 - seg.p0;
            edge_derivatives[1] = seg.p1 - seg.c1;
        }

        if (i > 0 || seg.is_closed) {
            const Gui::bezier_seg_t &prev_seg = (i > 0) ? segs[i - 1] : segs[count - 1];

            if (prev_seg.order == 1) {
                seg.dAdt1 = seg.p0 - prev_seg.p0;
            } else if (prev_seg.order == 2) {
                seg.dAdt1 = seg.p0 - prev_seg.c0;
            } else /*if (prev_seg.order == 3)*/ {
                seg.dAdt1 = seg.p0 - prev_seg.c1;
            }
        }

        if ((i < count - 1) || segs[0].is_closed) {
            const Gui::bezier_seg_t &next_seg = (i < count - 1) ? segs[i + 1] : segs[0];

            if (next_seg.order == 1) {
                seg.dBdt0 = next_seg.p1 - seg.p1;
            } else /*if(next_seg.order == 1 || next_seg.order == 2)*/ {
                seg.dBdt0 = next_seg.c0 - seg.p1;
            }
        }

        const double val = Dot(Normalize(edge_derivatives[0]), Normalize(seg.dAdt1));

        if (Dot(Normalize(edge_derivatives[0]), Normalize(seg.dAdt1)) < max_soft_dot) {
            seg.is_hard = true;
        }
    }
}

Gui::dist_result_t Gui::BezierSegmentDistance(const bezier_seg_t &seg, const Ren::Vec2d &p) {
    assert(seg.order >= 1 && seg.order <= 3);
    if (seg.order == 1) {
        return Bezier1Distance(seg.p0, seg.p1, p);
    } else if (seg.order == 2) {
        return Bezier2Distance(seg.p0, seg.c0, seg.p1, p);
    } else /*if (seg.order == 3)*/ {
        return Bezier3Distance(seg.p0, seg.c0, seg.c1, seg.p1, p);
    }
}

int Gui::FixSDFCollisions(uint8_t *img_data, int w, int h, int channels, int threshold) {
    using namespace GuiInternal;

    std::unique_ptr<int[]> marked_pixels(new int[w * h]);
    int marked_pixels_count = 0;

    // Mark problematic pixels
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            const int px_index = channels * (y * w + x);

            // pixel in question
            const uint8_t *p0 = &img_data[px_index];

            // pixel neighborhood
            const uint8_t
                *p0_e = &img_data[channels * ((y + 0) * w + (x - 1))],
                *p0_w = &img_data[channels * ((y + 0) * w + (x + 1))],
                *p0_n = &img_data[channels * ((y + 1) * w + (x + 0))],
                *p0_s = &img_data[channels * ((y - 1) * w + (x + 0))],
                *p0_se = &img_data[channels * ((y - 1) * w + (x - 1))],
                *p0_sw = &img_data[channels * ((y - 1) * w + (x + 1))],
                *p0_ne = &img_data[channels * ((y + 1) * w + (x - 1))],
                *p0_nw = &img_data[channels * ((y + 1) * w + (x + 1))];

            int ch_count = 0;

            for (int i = 0; i < 3; i++) {
                if (_abs(int(p0[i]) - p0_e[i]) > threshold || _abs(int(p0[i]) - p0_w[i]) > threshold ||
                    _abs(int(p0[i]) - p0_n[i]) > threshold || _abs(int(p0[i]) - p0_s[i]) > threshold ||
                    _abs(int(p0[i]) - p0_se[i]) > threshold || _abs(int(p0[i]) - p0_sw[i]) > threshold ||
                    _abs(int(p0[i]) - p0_ne[i]) > threshold || _abs(int(p0[i]) - p0_nw[i]) > threshold) {
                    ch_count++;
                }
            }

            if (ch_count >= 2) {
                marked_pixels[marked_pixels_count++] = px_index;
            }
        }
    }

    // Fix problematic pixels
    for (int i = 0; i < marked_pixels_count; i++) {
        uint8_t *p00 = &img_data[marked_pixels[i]];
        p00[0] = p00[1] = p00[2] = median(p00[0], p00[1], p00[2]);
    }

    return marked_pixels_count;
}

#undef _abs