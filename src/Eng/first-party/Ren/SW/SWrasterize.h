#ifndef SW_RASTERIZE_H
#define SW_RASTERIZE_H

#include <assert.h>
#include <math.h>

#include "SWframebuffer.h"
#include "SWprogram.h"

#define _swProjectOnScreen(f, p, out)                                                    \
    out[0] = (SWint)((f)->w * ((p)[0] + 1) / 2 + (SWfloat)0.5);                          \
    out[1] = (SWint)((f)->h * (-(p)[1] + 1) / 2 + (SWfloat)0.5);

#define _swProjectOnScreenF(f, p, out)                                                   \
    out[0] = floorf((f)->w * ((p)[0] + 1) / 2 + (SWfloat)0.5);                           \
    out[1] = floorf((f)->h * (-(p)[1] + 1) / 2 + (SWfloat)0.5);

sw_inline void _swBlendPixels(SWframebuffer *f, SWint ix, SWint iy, SWfloat f_out[4]) {
    SWfloat f_in[4];
    swFbufGetPixel_FRGBA(f, ix, iy, f_in);

    SWfloat k = 1 - f_out[3];

    f_out[0] = f_in[0] * k + f_out[0] * f_out[3];
    f_out[1] = f_in[1] * k + f_out[1] * f_out[3];
    f_out[2] = f_in[2] * k + f_out[2] * f_out[3];
}

sw_inline void _swProcessPixelLine(SWprogram *p, SWframebuffer *f, SWint x, SWint y,
                                   SWint b_depth_test, SWint b_depth_write,
                                   SWfloat *vs_out_res) {
    SWfloat f_out[4];
    SWint b_discard = 0;

    if (b_depth_test && !swFbufTestDepth(f, x, y, vs_out_res[2]))
        return;

    (*p->f_proc)(vs_out_res, p->uniforms, f_out, &b_discard);

    if (!b_discard) {
        if (b_depth_write) {
            swFbufSetDepth(f, x, y, vs_out_res[2]);
        }
        swFbufBGRA8888_SetPixel_FRGBA(f, x, y, f_out);
    }
}

sw_inline void _swProcessLine(SWprogram *p, SWframebuffer *f,
                              SWfloat vs_out1[SW_MAX_VTX_ATTRIBS],
                              SWfloat vs_out2[SW_MAX_VTX_ATTRIBS], SWint b_depth_test,
                              SWint b_depth_write) {
    SWfloat t = 0.0f;
    SWint x, p0[2], p1[2];

    _swProjectOnScreen(f, vs_out1, p0);
    _swProjectOnScreen(f, vs_out2, p1);

    SWfloat *RESTRICT fs_in1 = vs_out1;
    SWfloat *RESTRICT fs_in2 = vs_out2;

    SWfloat fs_res[SW_MAX_VTX_ATTRIBS], fs_delta[SW_MAX_VTX_ATTRIBS];

    SWint steep = 0;
    SWint w = f->w, h = f->h;
    if (sw_abs(p0[0] - p1[0]) < sw_abs(p0[1] - p1[1])) {
        sw_swap(p0[0], p0[1], SWint);
        sw_swap(p1[0], p1[1], SWint);
        sw_swap(w, h, SWint);
        steep = 1;
    }
    if (p0[0] > p1[0]) {
        sw_swap(p0[0], p1[0], SWint);
        sw_swap(p0[1], p1[1], SWint);

        sw_swap(fs_in1, fs_in2, SWfloat *);
    }

    SWint dx = p1[0] - p0[0], dy = p1[1] - p0[1];
    SWint derror2 = sw_abs(dy) * 2;
    SWint error2 = 0;
    SWint y = p0[1];
    SWfloat dt = (SWfloat)1.0 / (p1[0] - p0[0]);
    SWint i;

    for (i = 0; i < p->v_out_size; i++) {
        fs_delta[i] = dt * (fs_in2[i] - fs_in1[i]);
        fs_res[i] = fs_in1[i];
    }

    for (x = p0[0]; x <= p1[0]; x++) {
        if ((x < 0 || x >= w || y < 0 || y >= h) || /* test against viewport bounds */
            (fs_in1[2] < 0 || fs_in1[2] > 1)) {     /* test against depth bounds */
            /* skip fragment */
        } else {
            if (!steep) {
                _swProcessPixelLine(p, f, x, y, b_depth_test, b_depth_write, fs_res);
            } else {
                _swProcessPixelLine(p, f, y, x, b_depth_test, b_depth_write, fs_res);
            }
        }

        sw_add(fs_res, fs_delta, p->v_out_size);

        error2 += derror2;
        t += dt;

        if (error2 > dx) {
            y += (p1[1] > p0[1] ? 1 : -1);
            error2 -= dx * 2;
        }
    }
}

void _swProcessCurveRecursive(SWprogram *p, SWframebuffer *f,
                              SWfloat p1[SW_MAX_VTX_ATTRIBS],
                              SWfloat p2[SW_MAX_VTX_ATTRIBS],
                              SWfloat p3[SW_MAX_VTX_ATTRIBS],
                              SWfloat p4[SW_MAX_VTX_ATTRIBS], SWint b_depth_test,
                              SWint b_depth_write, SWfloat tolerance);

sw_inline void _swProcessCurve(SWprogram *p, SWframebuffer *f,
                               SWfloat vs_out[4][SW_MAX_VTX_ATTRIBS], SWint _1, SWint _2,
                               SWint _3, SWint _4, SWint b_depth_test,
                               SWint b_depth_write, SWfloat tolerance) {
    _swProcessCurveRecursive(p, f, vs_out[_1], vs_out[_2], vs_out[_3], vs_out[_4],
                             b_depth_test, b_depth_write, tolerance);
}

/******* Perspective correct interpolation *******/

sw_inline SWint _swIntepolateTri_correct(SWprogram *p, SWframebuffer *f,
                                         SWint b_depth_test, SWfloat *depth, SWint x,
                                         SWint y, SWfloat *RESTRICT f0,
                                         SWfloat *RESTRICT f1, SWfloat *RESTRICT f2,
                                         SWfloat uvw[3], SWfloat *ff) {
    SWint i;

    /* test against depth boundaries */
    if (ff[2] < 0 || ff[2] > 1) {
        return 0;
    }
    /* test against depth buffer */
    if (b_depth_test && !swFbufTestDepth(f, x, y, ff[2])) {
        return 0;
    }
    *depth = ff[2];

    for (i = 4; i < p->v_out_size; i++) {
        ff[i] = uvw[0] * f0[i] + uvw[1] * f1[i] + uvw[2] * f2[i];
    }

    return 1;
}

sw_inline void _swProcessPixel_correct(SWprogram *p, SWframebuffer *f, SWint ix, SWint iy,
                                       SWfloat uvw[3], SWfloat *f0, SWfloat *f1,
                                       SWfloat *f2, SWfloat *vs_interpolated,
                                       SWint b_depth_test, SWint b_depth_write,
                                       SWint b_blend) {
    SWfloat depth;
    if (_swIntepolateTri_correct(p, f, b_depth_test, &depth, ix, iy, f0, f1, f2, uvw,
                                 vs_interpolated)) {
        SWfloat f_out[4];
        SWint b_discard = 0;
        (*p->f_proc)(vs_interpolated, p->uniforms, f_out, &b_discard);

        if (!b_discard) {
            if (b_depth_write) {
                swFbufSetDepth(f, ix, iy, depth);
            }
            if (b_blend) {
                _swBlendPixels(f, ix, iy, f_out);
            }
            swFbufSetPixel_FRGBA(f, ix, iy, f_out);
        }
    }
}

sw_inline void _swProcessTriangle_correct(SWprogram *p, SWframebuffer *f,
                                          SWfloat vs_out[3][SW_MAX_VTX_ATTRIBS], SWint _0,
                                          SWint _1, SWint _2, SWint b_depth_test,
                                          SWint b_depth_write, SWint b_blend) {
    SWint x, y, p0[2], p1[2], p2[2];
    SWint min[2], max[2];
    SWfloat *pos0 = vs_out[_0], *pos1 = vs_out[_1], *pos2 = vs_out[_2];

    SWint d01[2], d12[2], d20[2];
    SWint area;
    SWfloat inv_area;
    SWint C1, C2, C3;
    SWfloat vs_interpolated[SW_MAX_VTX_ATTRIBS];

    _swProjectOnScreen(f, pos0, p0);
    _swProjectOnScreen(f, pos1, p1);
    _swProjectOnScreen(f, pos2, p2);

    min[0] = sw_min(p0[0], sw_min(p1[0], p2[0]));
    min[1] = sw_min(p0[1], sw_min(p1[1], p2[1]));
    max[0] = sw_max(p0[0], sw_max(p1[0], p2[0]));
    max[1] = sw_max(p0[1], sw_max(p1[1], p2[1]));

    if (min[0] >= f->w || min[1] >= f->h || max[0] < 0 || max[1] < 0)
        return;

    if (min[0] < 0)
        min[0] = 0;
    if (min[1] < 0)
        min[1] = 0;
    if (max[0] >= f->w)
        max[0] = f->w - 1;
    if (max[1] >= f->h)
        max[1] = f->h - 1;

    min[0] &= ~(SW_TILE_SIZE - 1);
    min[1] &= ~(SW_TILE_SIZE - 1);

    d01[0] = p0[0] - p1[0];
    d01[1] = p0[1] - p1[1];
    d12[0] = p1[0] - p2[0];
    d12[1] = p1[1] - p2[1];
    d20[0] = p2[0] - p0[0];
    d20[1] = p2[1] - p0[1];

    area = d01[0] * d20[1] - d20[0] * d01[1];

    if (area <= 0)
        return;

    inv_area = ((SWfloat)1) / area;

    C1 = d01[1] * p0[0] - d01[0] * p0[1];
    C2 = d12[1] * p1[0] - d12[0] * p1[1];
    C3 = d20[1] * p2[0] - d20[0] * p2[1];

    if (d01[1] < 0 || (d01[1] == 0 && d01[0] > 0))
        C1++;
    if (d12[1] < 0 || (d12[1] == 0 && d12[0] > 0))
        C2++;
    if (d20[1] < 0 || (d20[1] == 0 && d20[0] > 0))
        C3++;

    for (y = min[1]; y < max[1]; y += SW_TILE_SIZE) {
        for (x = min[0]; x < max[0]; x += SW_TILE_SIZE) {
            SWint x0 = x, x1 = sw_min(x + SW_TILE_SIZE - 1, f->w - 1), y0 = y,
                  y1 = sw_min(y + SW_TILE_SIZE - 1, f->h - 1);

            SWint full_tile =
                (x1 - x0 == SW_TILE_SIZE - 1) && (y1 - y0 == SW_TILE_SIZE - 1);

            SWint Cy1 = C1 + d01[0] * y0 - d01[1] * x0;
            SWint Cy2 = C2 + d12[0] * y0 - d12[1] * x0;
            SWint Cy3 = C3 + d20[0] * y0 - d20[1] * x0;

            SWint a = (Cy1 > 0 ? 1 : 0) + (C1 + d01[0] * y0 - d01[1] * x1 > 0 ? 2 : 0) +
                      (C1 + d01[0] * y1 - d01[1] * x0 > 0 ? 4 : 0) +
                      (C1 + d01[0] * y1 - d01[1] * x1 > 0 ? 8 : 0);

            SWint b = (Cy2 > 0 ? 1 : 0) + (C2 + d12[0] * y0 - d12[1] * x1 > 0 ? 2 : 0) +
                      (C2 + d12[0] * y1 - d12[1] * x0 > 0 ? 4 : 0) +
                      (C2 + d12[0] * y1 - d12[1] * x1 > 0 ? 8 : 0);

            SWint c = (Cy3 > 0 ? 1 : 0) + (C3 + d20[0] * y0 - d20[1] * x1 > 0 ? 2 : 0) +
                      (C3 + d20[0] * y1 - d20[1] * x0 > 0 ? 4 : 0) +
                      (C3 + d20[0] * y1 - d20[1] * x1 > 0 ? 8 : 0);

            if (a == 0 || b == 0 || c == 0)
                continue;

            if (a == 15 && b == 15 && c == 15 && full_tile) {
                SWint ix, iy;
                for (iy = y; iy < y + SW_TILE_SIZE; iy++) {
                    SWint Cx1 = Cy1, Cx3 = Cy3;
                    for (ix = x; ix < x + SW_TILE_SIZE; ix++) {
                        SWfloat uvw[3] = {0, Cx3 * inv_area, Cx1 * inv_area};
                        uvw[0] = 1 - uvw[1] - uvw[2];

                        vs_interpolated[2] =
                            uvw[0] * pos0[2] + uvw[1] * pos1[2] + uvw[2] * pos2[2];

                        SWfloat inv_w =
                            1 / (uvw[0] * pos0[3] + uvw[1] * pos1[3] + uvw[2] * pos2[3]);
                        uvw[0] *= inv_w;
                        uvw[1] *= inv_w;
                        uvw[2] *= inv_w;

                        _swProcessPixel_correct(p, f, ix, iy, uvw, pos0, pos1, pos2,
                                                vs_interpolated, b_depth_test,
                                                b_depth_write, b_blend);

                        Cx1 -= d01[1];
                        Cx3 -= d20[1];
                    }
                    Cy1 += d01[0];
                    Cy3 += d20[0];
                }
            } else {
                SWint ix, iy;
                for (iy = y; iy <= y1; iy++) {
                    SWint Cx1 = Cy1, Cx2 = Cy2, Cx3 = Cy3;
                    for (ix = x; ix <= x1; ix++) {
                        if (Cx1 > 0 && Cx2 > 0 && Cx3 > 0) {
                            SWfloat uvw[3] = {0, Cx3 * inv_area, Cx1 * inv_area};
                            uvw[0] = 1 - uvw[1] - uvw[2];

                            vs_interpolated[2] =
                                uvw[0] * pos0[2] + uvw[1] * pos1[2] + uvw[2] * pos2[2];

                            SWfloat inv_w = 1 / (uvw[0] * pos0[3] + uvw[1] * pos1[3] +
                                                 uvw[2] * pos2[3]);
                            uvw[0] *= inv_w;
                            uvw[1] *= inv_w;
                            uvw[2] *= inv_w;

                            _swProcessPixel_correct(p, f, ix, iy, uvw, pos0, pos1, pos2,
                                                    vs_interpolated, b_depth_test,
                                                    b_depth_write, b_blend);
                        }
                        Cx1 -= d01[1];
                        Cx2 -= d12[1];
                        Cx3 -= d20[1];
                    }
                    Cy1 += d01[0];
                    Cy2 += d12[0];
                    Cy3 += d20[0];
                }
            }
        }
    }
}

/******* Affine interpolation *******/

sw_inline SWint _swIntepolateTri_nocorrect(SWframebuffer *f, SWint b_depth_test,
                                           SWfloat *depth, SWint x, SWint y,
                                           SWfloat *ff) {
    /* test against depth boundaries */
    if (ff[2] < 0 || ff[2] > 1) {
        return 0;
    }
    /* test against depth buffer */
    if (b_depth_test && !swFbufTestDepth(f, x, y, ff[2])) {
        return 0;
    }
    *depth = ff[2];
    return 1;
}

sw_inline void _swProcessPixel_nocorrect(SWprogram *p, SWframebuffer *f, SWint ix,
                                         SWint iy,
                                         SWfloat vs_interpolated[SW_MAX_VTX_ATTRIBS],
                                         SWint b_depth_test, SWint b_depth_write,
                                         SWint b_blend) {
    SWfloat depth;
    if (_swIntepolateTri_nocorrect(f, b_depth_test, &depth, ix, iy, vs_interpolated)) {
        SWfloat f_out[4];
        SWint b_discard = 0;
        (*p->f_proc)(vs_interpolated, p->uniforms, f_out, &b_discard);

        if (!b_discard) {
            if (b_depth_write) {
                swFbufSetDepth(f, ix, iy, depth);
            }
            if (b_blend) {
                _swBlendPixels(f, ix, iy, f_out);
            }
            swFbufSetPixel_FRGBA(f, ix, iy, f_out);
        }
    }
}

sw_inline void _swProcessTriangle_nocorrect(SWprogram *p, SWframebuffer *f,
                                            SWfloat vs_out[3][SW_MAX_VTX_ATTRIBS],
                                            SWint _0, SWint _1, SWint _2,
                                            SWint b_depth_test, SWint b_depth_write,
                                            SWint b_blend) {
    SWint i, x, y, p0[2], p1[2], p2[2];
    SWint min[2], max[2];
    SWfloat *pos0 = vs_out[_0], *pos1 = vs_out[_1], *pos2 = vs_out[_2];

    SWint d01[2], d12[2], d20[2];
    SWint area;
    SWfloat inv_area;
    SWint C1, C2, C3;
    SWfloat vs_interpolated[SW_MAX_VTX_ATTRIBS];
    SWfloat vs_d10[SW_MAX_VTX_ATTRIBS], vs_d20[SW_MAX_VTX_ATTRIBS];
    const SWint num_attributes = p->v_out_size;

    _swProjectOnScreen(f, pos0, p0);
    _swProjectOnScreen(f, pos1, p1);
    _swProjectOnScreen(f, pos2, p2);

    min[0] = sw_min(p0[0], sw_min(p1[0], p2[0]));
    min[1] = sw_min(p0[1], sw_min(p1[1], p2[1]));
    max[0] = sw_max(p0[0], sw_max(p1[0], p2[0]));
    max[1] = sw_max(p0[1], sw_max(p1[1], p2[1]));

    if (min[0] >= f->w || min[1] >= f->h || max[0] < 0 || max[1] < 0)
        return;

    if (min[0] < 0)
        min[0] = 0;
    if (min[1] < 0)
        min[1] = 0;
    if (max[0] >= f->w)
        max[0] = f->w - 1;
    if (max[1] >= f->h)
        max[1] = f->h - 1;

    min[0] &= ~(SW_TILE_SIZE - 1);
    min[1] &= ~(SW_TILE_SIZE - 1);

    d01[0] = p0[0] - p1[0];
    d01[1] = p0[1] - p1[1];
    d12[0] = p1[0] - p2[0];
    d12[1] = p1[1] - p2[1];
    d20[0] = p2[0] - p0[0];
    d20[1] = p2[1] - p0[1];

    area = d01[0] * d20[1] - d20[0] * d01[1];

    if (area <= 0)
        return;

    inv_area = ((SWfloat)1) / area;

    C1 = d01[1] * p0[0] - d01[0] * p0[1];
    C2 = d12[1] * p1[0] - d12[0] * p1[1];
    C3 = d20[1] * p2[0] - d20[0] * p2[1];

    if (d01[1] < 0 || (d01[1] == 0 && d01[0] > 0))
        C1++;
    if (d12[1] < 0 || (d12[1] == 0 && d12[0] > 0))
        C2++;
    if (d20[1] < 0 || (d20[1] == 0 && d20[0] > 0))
        C3++;

    for (i = 0; i < num_attributes; i++) {
        vs_d10[i] = vs_out[_1][i] - vs_out[_0][i];
        vs_d20[i] = vs_out[_2][i] - vs_out[_0][i];
    }

    for (y = min[1]; y < max[1]; y += SW_TILE_SIZE) {
        for (x = min[0]; x < max[0]; x += SW_TILE_SIZE) {
            SWint x0 = x, x1 = sw_min(x + SW_TILE_SIZE - 1, f->w - 1), y0 = y,
                  y1 = sw_min(y + SW_TILE_SIZE - 1, f->h - 1);

            SWint full_tile =
                (x1 - x0 == SW_TILE_SIZE - 1) && (y1 - y0 == SW_TILE_SIZE - 1);

            SWint Cy1 = C1 + d01[0] * y0 - d01[1] * x0;
            SWint Cy2 = C2 + d12[0] * y0 - d12[1] * x0;
            SWint Cy3 = C3 + d20[0] * y0 - d20[1] * x0;

            SWint Cy1_10 = C1 + d01[0] * y0 - d01[1] * x1;
            SWint Cy2_10 = C2 + d12[0] * y0 - d12[1] * x1;
            SWint Cy3_10 = C3 + d20[0] * y0 - d20[1] * x1;

            SWint Cy1_01 = C1 + d01[0] * y1 - d01[1] * x0;
            SWint Cy2_01 = C2 + d12[0] * y1 - d12[1] * x0;
            SWint Cy3_01 = C3 + d20[0] * y1 - d20[1] * x0;

            SWint Cy1_11 = C1 + d01[0] * y1 - d01[1] * x1;
            SWint Cy2_11 = C2 + d12[0] * y1 - d12[1] * x1;
            SWint Cy3_11 = C3 + d20[0] * y1 - d20[1] * x1;

            SWint a = ((Cy1 > 0) << 0) | ((Cy1_10 > 0) << 1) | ((Cy1_01 > 0) << 2) |
                      ((Cy1_11 > 0) << 3);
            SWint b = ((Cy2 > 0) << 0) | ((Cy2_10 > 0) << 1) | ((Cy2_01 > 0) << 2) |
                      ((Cy2_11 > 0) << 3);
            SWint c = ((Cy3 > 0) << 0) | ((Cy3_10 > 0) << 1) | ((Cy3_01 > 0) << 2) |
                      ((Cy3_11 > 0) << 3);

            if (a == 0 || b == 0 || c == 0)
                continue;

            SWint full_cover = (a == 15 && b == 15 && c == 15) && full_tile;

            /////

            SWfloat f00_z =
                pos0[2] + Cy3 * inv_area * vs_d10[2] + Cy1 * inv_area * vs_d20[2];
            SWfloat f10_z =
                pos0[2] + Cy3_10 * inv_area * vs_d10[2] + Cy1_10 * inv_area * vs_d20[2];
            SWfloat f01_z =
                pos0[2] + Cy3_01 * inv_area * vs_d10[2] + Cy1_01 * inv_area * vs_d20[2];
            SWfloat f11_z =
                pos0[2] + Cy3_11 * inv_area * vs_d10[2] + Cy1_11 * inv_area * vs_d20[2];

            SWfloat zmin = sw_min(f00_z, sw_min(f10_z, sw_min(f01_z, f11_z)));
            SWfloat zmax = sw_max(f00_z, sw_max(f10_z, sw_max(f01_z, f11_z)));

            SWint depth_test = b_depth_test;
            SWoccresult res = SW_PARTIAL;
            if (depth_test) {
                res = swFbufTestTileRange(f, x0, y0, zmin, zmax);
                if (res == SW_OCCLUDED) {
                    continue;
                } else if (res == SW_NONOCCLUDED) {
                    depth_test = 0;
                }
            }

            if (b_depth_write) {
                if (full_cover && res == SW_NONOCCLUDED) {
                    swFbufSetTileRange(f, x0, y0, zmin, zmax);
                } else {
                    swFbufUpdateTileRange(f, x0, y0, zmin, zmax);
                }
            }

            SWfloat vs_dx[SW_MAX_VTX_ATTRIBS], vs_dy[SW_MAX_VTX_ATTRIBS];
            for (i = 0; i < num_attributes; i++) {
                vs_dx[i] = -d20[1] * inv_area * vs_d10[i] - d01[1] * inv_area * vs_d20[i];
                vs_dy[i] = d20[0] * inv_area * vs_d10[i] + d01[0] * inv_area * vs_d20[i] -
                           SW_TILE_SIZE * vs_dx[i];
                vs_interpolated[i] =
                    pos0[i] + Cy3 * inv_area * vs_d10[i] + Cy1 * inv_area * vs_d20[i];
            }

            /////

            if (full_cover) {
                SWint ix, iy;
                for (iy = y; iy < y + SW_TILE_SIZE; iy++) {
                    SWint Cx1 = Cy1, Cx3 = Cy3;
                    for (ix = x; ix < x + SW_TILE_SIZE; ix++) {
                        _swProcessPixel_nocorrect(p, f, ix, iy, vs_interpolated,
                                                  depth_test, b_depth_write, b_blend);

                        sw_add(vs_interpolated, vs_dx, num_attributes);

                        Cx1 -= d01[1];
                        Cx3 -= d20[1];
                    }

                    sw_add(vs_interpolated, vs_dy, num_attributes);

                    Cy1 += d01[0];
                    Cy3 += d20[0];
                }
            } else {
                SWint ix, iy;
                for (iy = y; iy <= y1; iy++) {
                    SWint Cx1 = Cy1, Cx2 = Cy2, Cx3 = Cy3;
                    for (ix = x; ix <= x1; ix++) {
                        if (Cx1 > 0 && Cx2 > 0 && Cx3 > 0) {
                            _swProcessPixel_nocorrect(p, f, ix, iy, vs_interpolated,
                                                      depth_test, b_depth_write, b_blend);
                        }

                        sw_add(vs_interpolated, vs_dx, num_attributes);

                        Cx1 -= d01[1];
                        Cx2 -= d12[1];
                        Cx3 -= d20[1];
                    }

                    sw_add(vs_interpolated, vs_dy, num_attributes);

                    Cy1 += d01[0];
                    Cy2 += d12[0];
                    Cy3 += d20[0];
                }
            }
        }
    }
}

/*******************************/

sw_inline SWint _swIntepolateTri_fast(SWframebuffer *f, SWint b_depth_test,
                                      SWfloat *depth, SWint x, SWint y, SWfloat *ff) {
    /* test against depth boundaries */
    if (ff[2] < 0 || ff[2] > 1) {
        return 0;
    }
    /* test against depth buffer */
    if (b_depth_test && !swFbufTestDepth(f, x, y, ff[2])) {
        return 0;
    }
    *depth = ff[2];
    return 1;
}

sw_inline void _swProcessPixel_fast(SWprogram *p, SWframebuffer *f, SWint ix, SWint iy,
                                    SWfloat vs_interpolated[SW_MAX_VTX_ATTRIBS],
                                    SWint b_depth_test, SWint b_depth_write,
                                    SWint b_blend) {
    SWfloat depth;
    if (_swIntepolateTri_fast(f, b_depth_test, &depth, ix, iy, vs_interpolated)) {
        SWfloat f_out[4];
        SWint b_discard = 0;
        (*p->f_proc)(vs_interpolated, p->uniforms, f_out, &b_discard);

        if (!b_discard) {
            if (b_depth_write) {
                swFbufSetDepth(f, ix, iy, depth);
            }
            if (b_blend) {
                _swBlendPixels(f, ix, iy, f_out);
            }
            swFbufSetPixel_FRGBA(f, ix, iy, f_out);
        }
    }
}

sw_inline void _swProcessTriangle_fast(SWprogram *p, SWframebuffer *f,
                                       SWfloat vs_out[3][SW_MAX_VTX_ATTRIBS], SWint _0,
                                       SWint _1, SWint _2, SWint b_depth_test,
                                       SWint b_depth_write, SWint b_blend) {
    SWint i, x, y, p0[2], p1[2], p2[2];
    SWint min[2], max[2];
    SWfloat *pos0 = vs_out[_0], *pos1 = vs_out[_1], *pos2 = vs_out[_2];

    SWint d01[2], d12[2], d20[2];
    SWint area;
    SWfloat inv_area;
    SWint C1, C2, C3;
    SWfloat vs_interpolated[SW_MAX_VTX_ATTRIBS];
    SWfloat vs_d10[SW_MAX_VTX_ATTRIBS], vs_d20[SW_MAX_VTX_ATTRIBS];
    const SWint num_attributes = p->v_out_size;

    _swProjectOnScreen(f, pos0, p0);
    _swProjectOnScreen(f, pos1, p1);
    _swProjectOnScreen(f, pos2, p2);

    min[0] = sw_min(p0[0], sw_min(p1[0], p2[0]));
    min[1] = sw_min(p0[1], sw_min(p1[1], p2[1]));
    max[0] = sw_max(p0[0], sw_max(p1[0], p2[0]));
    max[1] = sw_max(p0[1], sw_max(p1[1], p2[1]));

    if (min[0] >= f->w || min[1] >= f->h || max[0] < 0 || max[1] < 0)
        return;

    if (min[0] < 0)
        min[0] = 0;
    if (min[1] < 0)
        min[1] = 0;
    if (max[0] >= f->w)
        max[0] = f->w - 1;
    if (max[1] >= f->h)
        max[1] = f->h - 1;

    min[0] &= ~(SW_TILE_SIZE - 1);
    min[1] &= ~(SW_TILE_SIZE - 1);

    d01[0] = p0[0] - p1[0];
    d01[1] = p0[1] - p1[1];
    d12[0] = p1[0] - p2[0];
    d12[1] = p1[1] - p2[1];
    d20[0] = p2[0] - p0[0];
    d20[1] = p2[1] - p0[1];

    area = d01[0] * d20[1] - d20[0] * d01[1];

    if (area <= 0)
        return;

    inv_area = ((SWfloat)1) / area;

    C1 = d01[1] * p0[0] - d01[0] * p0[1];
    C2 = d12[1] * p1[0] - d12[0] * p1[1];
    C3 = d20[1] * p2[0] - d20[0] * p2[1];

    if (d01[1] < 0 || (d01[1] == 0 && d01[0] > 0))
        C1++;
    if (d12[1] < 0 || (d12[1] == 0 && d12[0] > 0))
        C2++;
    if (d20[1] < 0 || (d20[1] == 0 && d20[0] > 0))
        C3++;

    for (i = 0; i < num_attributes; i++) {
        vs_d10[i] = vs_out[_1][i] - vs_out[_0][i];
        vs_d20[i] = vs_out[_2][i] - vs_out[_0][i];
    }

    for (y = min[1]; y < max[1]; y += SW_TILE_SIZE) {
        for (x = min[0]; x < max[0]; x += SW_TILE_SIZE) {
            SWint x0 = x, x1 = sw_min(x + SW_TILE_SIZE - 1, f->w - 1), y0 = y,
                  y1 = sw_min(y + SW_TILE_SIZE - 1, f->h - 1);

            SWint full_tile =
                (x1 - x0 == SW_TILE_SIZE - 1) && (y1 - y0 == SW_TILE_SIZE - 1);

            SWint Cy1_00 = C1 + d01[0] * y0 - d01[1] * x0,
                  Cy2_00 = C2 + d12[0] * y0 - d12[1] * x0,
                  Cy3_00 = C3 + d20[0] * y0 - d20[1] * x0;

            SWint Cy1_10 = C1 + d01[0] * y0 - d01[1] * x1,
                  Cy2_10 = C2 + d12[0] * y0 - d12[1] * x1,
                  Cy3_10 = C3 + d20[0] * y0 - d20[1] * x1;

            SWint Cy1_01 = C1 + d01[0] * y1 - d01[1] * x0,
                  Cy2_01 = C2 + d12[0] * y1 - d12[1] * x0,
                  Cy3_01 = C3 + d20[0] * y1 - d20[1] * x0;

            SWint Cy1_11 = C1 + d01[0] * y1 - d01[1] * x1,
                  Cy2_11 = C2 + d12[0] * y1 - d12[1] * x1,
                  Cy3_11 = C3 + d20[0] * y1 - d20[1] * x1;

            SWint a = ((Cy1_00 > 0) << 0) | ((Cy1_10 > 0) << 1) | ((Cy1_01 > 0) << 2) |
                      ((Cy1_11 > 0) << 3);
            SWint b = ((Cy2_00 > 0) << 0) | ((Cy2_10 > 0) << 1) | ((Cy2_01 > 0) << 2) |
                      ((Cy2_11 > 0) << 3);
            SWint c = ((Cy3_00 > 0) << 0) | ((Cy3_10 > 0) << 1) | ((Cy3_01 > 0) << 2) |
                      ((Cy3_11 > 0) << 3);

            if (a == 0 || b == 0 || c == 0)
                continue;

            SWint full_cover = (a == 15 && b == 15 && c == 15) && full_tile;

            /////

            SWfloat f00[SW_MAX_VTX_ATTRIBS], f10[SW_MAX_VTX_ATTRIBS],
                f01[SW_MAX_VTX_ATTRIBS], f11[SW_MAX_VTX_ATTRIBS];

            SWfloat step_x[SW_MAX_VTX_ATTRIBS] = {0};
            // SWfloat step_y_left[SW_MAX_VTX_ATTRIBS],
            //      step_y_right[SW_MAX_VTX_ATTRIBS];
            SWfloat *step_y_left = f01, *step_y_right = f11;

            f00[2] =
                pos0[2] + Cy3_00 * inv_area * vs_d10[2] + Cy1_00 * inv_area * vs_d20[2];
            f10[2] =
                pos0[2] + Cy3_10 * inv_area * vs_d10[2] + Cy1_10 * inv_area * vs_d20[2];
            f01[2] =
                pos0[2] + Cy3_01 * inv_area * vs_d10[2] + Cy1_01 * inv_area * vs_d20[2];
            f11[2] =
                pos0[2] + Cy3_11 * inv_area * vs_d10[2] + Cy1_11 * inv_area * vs_d20[2];

            SWfloat zmin = sw_min(f00[2], sw_min(f10[2], sw_min(f01[2], f11[2])));
            SWfloat zmax = sw_max(f00[2], sw_max(f10[2], sw_max(f01[2], f11[2])));

            SWint depth_test = b_depth_test;
            SWoccresult res = SW_PARTIAL;
            if (depth_test) {
                res = swFbufTestTileRange(f, x0, y0, zmin, zmax);
                if (res == SW_OCCLUDED) {
                    continue;
                } else if (res == SW_NONOCCLUDED) {
                    depth_test = 0;
                }
            }

            if (b_depth_write) {
                if (full_cover && res == SW_NONOCCLUDED) {
                    swFbufSetTileRange(f, x0, y0, zmin, zmax);
                } else {
                    swFbufUpdateTileRange(f, x0, y0, zmin, zmax);
                }
            }

            step_y_left[2] = (f01[2] - f00[2]) * SW_INV_TILE_STEP;
            step_y_right[2] = (f11[2] - f10[2]) * SW_INV_TILE_STEP;

            /*******/

            SWfloat k1_00 = Cy3_00 * inv_area, k2_00 = Cy1_00 * inv_area;
            SWfloat inv_w_00 = 1 / (pos0[3] + k1_00 * vs_d10[3] + k2_00 * vs_d20[3]);

            SWfloat k1_10 = Cy3_10 * inv_area, k2_10 = Cy1_10 * inv_area;
            SWfloat inv_w_10 = 1 / (pos0[3] + k1_10 * vs_d10[3] + k2_10 * vs_d20[3]);

            SWfloat k1_01 = Cy3_01 * inv_area, k2_01 = Cy1_01 * inv_area;
            SWfloat inv_w_01 = 1 / (pos0[3] + k1_01 * vs_d10[3] + k2_01 * vs_d20[3]);

            SWfloat k1_11 = Cy3_11 * inv_area, k2_11 = Cy1_11 * inv_area;
            SWfloat inv_w_11 = 1 / (pos0[3] + k1_11 * vs_d10[3] + k2_11 * vs_d20[3]);

            for (i = 0; i < num_attributes; i++) {
                if (i == 2)
                    continue;

                f00[i] = pos0[i] + k1_00 * vs_d10[i] + k2_00 * vs_d20[i];
                f10[i] = pos0[i] + k1_10 * vs_d10[i] + k2_10 * vs_d20[i];
                f01[i] = pos0[i] + k1_01 * vs_d10[i] + k2_01 * vs_d20[i];
                f11[i] = pos0[i] + k1_11 * vs_d10[i] + k2_11 * vs_d20[i];

                f00[i] *= inv_w_00;
                f10[i] *= inv_w_10;
                f01[i] *= inv_w_01;
                f11[i] *= inv_w_11;

                step_y_left[i] = (f01[i] - f00[i]) * SW_INV_TILE_STEP;
                step_y_right[i] = (f11[i] - f10[i]) * SW_INV_TILE_STEP;
            }

            /*******/

            if (full_cover) {
                SWint ix, iy;
                for (iy = y; iy < y + SW_TILE_SIZE; iy++) {
                    SWint attr;
                    for (attr = 0; attr < num_attributes; attr++) {
                        vs_interpolated[attr] = f00[attr];
                        step_x[attr] = (f10[attr] - f00[attr]) * SW_INV_TILE_STEP;
                    }

                    for (ix = x; ix < x + SW_TILE_SIZE; ix++) {
                        _swProcessPixel_fast(p, f, ix, iy, vs_interpolated, depth_test,
                                             b_depth_write, b_blend);

                        sw_add(vs_interpolated, step_x, num_attributes);
                    }

                    sw_add(f00, step_y_left, num_attributes);
                    sw_add(f10, step_y_right, num_attributes);
                }
            } else {
                SWint ix, iy;
                for (iy = y; iy <= y1; iy++) {
                    SWint Cx1 = Cy1_00, Cx2 = Cy2_00, Cx3 = Cy3_00;
                    SWint attr;
                    for (attr = 0; attr < num_attributes; attr++) {
                        vs_interpolated[attr] = f00[attr];
                        step_x[i] = (f10[attr] - f00[attr]) * SW_INV_TILE_STEP;
                    }

                    for (ix = x; ix <= x1; ix++) {
                        if (Cx1 > 0 && Cx2 > 0 && Cx3 > 0) {
                            _swProcessPixel_fast(p, f, ix, iy, vs_interpolated,
                                                 depth_test, b_depth_write, b_blend);
                        }

                        sw_add(vs_interpolated, step_x, num_attributes);

                        Cx1 -= d01[1];
                        Cx2 -= d12[1];
                        Cx3 -= d20[1];
                    }

                    sw_add(f00, step_y_left, num_attributes);
                    sw_add(f10, step_y_right, num_attributes);

                    Cy1_00 += d01[0];
                    Cy2_00 += d12[0];
                    Cy3_00 += d20[0];
                }
            }
        }
    }
}

#define PLANE_DOT(x, y)                                                                  \
    ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2] + (x)[3] * (y)[3])

#define DIFFERENT_SIGNS(x, y) (((x) <= 0 && (y) > 0) || ((x) > 0 && (y) <= 0))

#define LERP(x, y, t) ((y) + ((x) - (y)) * (t))

sw_inline SWint _swClipAndTransformToNDC_Line(SWfloat vs_in[2][SW_MAX_VTX_ATTRIBS],
                                              SWfloat vs_out[2][SW_MAX_VTX_ATTRIBS],
                                              const SWint num_attrs,
                                              const SWint num_corr_attrs) {
    SWfloat clip_planes[6][4] = {{-1, 0, 0, 1}, /* pos x */
                                 {1, 0, 0, 1},  /* neg x */
                                 {0, -1, 0, 1}, /* pos y */
                                 {0, 1, 0, 1},  /* neg y */
                                 {0, 0, -1, 1}, /* pos z */
                                 {0, 0, 0, 1}}; /* neg z */
    SWint clip_plane_bits[6] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
    SWint clip_mask = 0;
    SWint i, j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 6; j++) {
            if (PLANE_DOT(vs_in[i], clip_planes[j]) < 0)
                clip_mask |= clip_plane_bits[j];
        }
    }

    SWfloat t0 = 0, t1 = 0;

    for (i = 0; i < 6; i++) {
        if (clip_mask & clip_plane_bits[i]) {
            const SWfloat dp0 = PLANE_DOT(vs_in[0], clip_planes[i]);
            const SWfloat dp1 = PLANE_DOT(vs_in[1], clip_planes[i]);

            if (dp0 < 0 && dp1 < 0) {
                return 0;
            }

            if (dp1 < 0) {
                SWfloat t = dp1 / (dp1 - dp0);
                if (t > t1)
                    t1 = t;
            } else if (dp0 < 0) {
                SWfloat t = dp0 / (dp0 - dp1);
                if (t > t0)
                    t0 = t;
            }

            if (t0 + t1 >= 1) {
                return 0;
            }
        }
    }

    SWint ii;

    if (clip_mask || vs_in != vs_out) {
        for (ii = 0; ii < num_attrs; ii++) {
            SWfloat f1 = vs_in[0][ii], f2 = vs_in[1][ii];
            vs_out[0][ii] = LERP(f2, f1, t0);
            vs_out[1][ii] = LERP(f1, f2, t1);
        }
    }

    for (i = 0; i < 2; i++) {
        SWfloat inv_posw = 1 / vs_out[i][3];
        for (ii = 0; ii < num_corr_attrs; ii++) {
            vs_out[i][ii] *= inv_posw;
        }
        vs_out[i][3] = inv_posw;
    }

    return 1;
}

sw_inline SWint _swClipAndTransformToNDC_Tri(SWfloat vs_in[][SW_MAX_VTX_ATTRIBS],
                                             SWfloat vs_out[16][SW_MAX_VTX_ATTRIBS],
                                             SWint _0, SWint _1, SWint _2,
                                             SWint out_verts[16], SWint num_attrs,
                                             SWint num_corr_attrs) {
    const SWfloat clip_planes[6][4] = {{-1, 0, 0, 1}, {1, 0, 0, 1},  {0, -1, 0, 1},
                                       {0, 1, 0, 1},  {0, 0, -1, 1}, {0, 0, 0, 1}};
    const SWint clip_plane_bits[6] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
    SWint clip_mask = 0, num_verts = 3;
    SWint i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 6; j++) {
            if (PLANE_DOT(vs_in[i], clip_planes[j]) < -0.01f)
                clip_mask |= clip_plane_bits[j];
        }
    }

    if (vs_in != vs_out) {
        for (i = 0; i < 3; i++) {
            for (j = 0; j < num_attrs; j++) {
                vs_out[i][j] = vs_in[i][j];
            }
        }
    }

    SWint list1[16] = {_0, _1, _2};
    SWint *in_list = list1, *out_list = out_verts;
    SWint n = num_verts;

#define EMIT_VERTEX(ndx1, ndx2, t)                                                       \
    {                                                                                    \
        SWint ii;                                                                        \
        for (ii = 0; ii < num_attrs; ii++) {                                             \
            vs_out[num_verts][ii] = LERP(vs_out[ndx1][ii], vs_out[ndx2][ii], t);         \
        }                                                                                \
        num_verts++;                                                                     \
    }

    for (i = 0; i < 6; i++) {
        if (clip_mask & clip_plane_bits[i]) {
            SWint out_count = 0;
            SWint ndx_prev = in_list[0];
            SWfloat dp_prev = PLANE_DOT(vs_out[ndx_prev], clip_planes[i]);

            in_list[n] = in_list[0];
            for (j = 1; j <= n; j++) {
                SWint ndx = in_list[j];
                SWfloat dp = PLANE_DOT(vs_out[ndx], clip_planes[i]);
                if (dp_prev >= 0) {
                    out_list[out_count++] = ndx_prev;
                }

                if (DIFFERENT_SIGNS(dp, dp_prev)) {
                    if (dp < 0) {
                        SWfloat t = dp / (dp - dp_prev);
                        EMIT_VERTEX(ndx_prev, ndx, t);
                    } else {
                        SWfloat t = dp_prev / (dp_prev - dp);
                        EMIT_VERTEX(ndx, ndx_prev, t);
                    }
                    out_list[out_count++] = num_verts - 1;
                }

                ndx_prev = ndx;
                dp_prev = dp;
            }

            if (out_count < 3) {
                return 0;
            }

            sw_swap(in_list, out_list, SWint *);
            n = out_count;
        }
    }

    if (in_list != out_verts) {
        for (i = 0; i < n; i++) {
            out_verts[i] = in_list[i];
        }
    }

    for (i = 0; i < n; i++) {
        SWint ii, ndx = in_list[i];
        SWfloat inv_posw = 1 / vs_out[ndx][3];
        for (ii = 0; ii < num_corr_attrs; ii++) {
            vs_out[ndx][ii] *= inv_posw;
        }
        vs_out[ndx][3] = inv_posw;
    }

    return n;

#undef EMIT_VERTEX
}

sw_inline SWint _swClipAndTransformToNDC_Tri_Simple(SWfloat vs_in[][4],
                                                    SWfloat vs_out[16][4], SWint _0,
                                                    SWint _1, SWint _2, SWfloat zmax,
                                                    SWint out_verts[16]) {
    const SWfloat clip_planes[6][4] = {{-1, 0, 0, 1}, {1, 0, 0, 1},     {0, -1, 0, 1},
                                       {0, 1, 0, 1},  {0, 0, -1, zmax}, {0, 0, 1, 1}};
    const SWint clip_plane_bits[6] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
    SWint clip_mask = 0, num_verts = 3;
    SWint i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 6; j++) {
            if (PLANE_DOT(vs_in[i], clip_planes[j]) < -0.01f) {
                clip_mask |= clip_plane_bits[j];
            }
        }
    }

    if (vs_in != vs_out) {
        for (i = 0; i < 3; i++) {
            vs_out[i][0] = vs_in[i][0];
            vs_out[i][1] = vs_in[i][1];
            vs_out[i][2] = vs_in[i][2];
            vs_out[i][3] = vs_in[i][3];
        }
    }

    SWint list1[16] = {_0, _1, _2};
    SWint *in_list = list1, *out_list = out_verts;
    SWint n = num_verts;

#define EMIT_VERTEX(ndx1, ndx2, t)                                                       \
    vs_out[num_verts][0] = LERP(vs_out[ndx1][0], vs_out[ndx2][0], t);                    \
    vs_out[num_verts][1] = LERP(vs_out[ndx1][1], vs_out[ndx2][1], t);                    \
    vs_out[num_verts][2] = LERP(vs_out[ndx1][2], vs_out[ndx2][2], t);                    \
    vs_out[num_verts][3] = LERP(vs_out[ndx1][3], vs_out[ndx2][3], t);                    \
    num_verts++;

    for (i = 0; i < 6; i++) {
        if (clip_mask & clip_plane_bits[i]) {
            SWint out_count = 0;
            SWint ndx_prev = in_list[0];
            SWfloat dp_prev = PLANE_DOT(vs_out[ndx_prev], clip_planes[i]);

            in_list[n] = in_list[0];
            for (j = 1; j <= n; j++) {
                SWint ndx = in_list[j];
                SWfloat dp = PLANE_DOT(vs_out[ndx], clip_planes[i]);
                if (dp_prev >= 0) {
                    out_list[out_count++] = ndx_prev;
                }

                if (DIFFERENT_SIGNS(dp, dp_prev)) {
                    if (dp < 0) {
                        SWfloat t = dp / (dp - dp_prev);
                        EMIT_VERTEX(ndx_prev, ndx, t);
                    } else {
                        SWfloat t = dp_prev / (dp_prev - dp);
                        EMIT_VERTEX(ndx, ndx_prev, t);
                    }
                    out_list[out_count++] = num_verts - 1;
                }

                ndx_prev = ndx;
                dp_prev = dp;
            }

            if (out_count < 3) {
                return 0;
            }

            sw_swap(in_list, out_list, SWint *);
            n = out_count;
        }
    }

    if (in_list != out_verts) {
        for (i = 0; i < n; i++) {
            out_verts[i] = in_list[i];
        }
    }

    for (i = 0; i < n; i++) {
        SWint ndx = in_list[i];
        vs_out[ndx][3] = 1 / vs_out[ndx][3];
        vs_out[ndx][0] *= vs_out[ndx][3];
        vs_out[ndx][1] *= vs_out[ndx][3];
        vs_out[ndx][2] *= vs_out[ndx][3];
    }

    return n;

#undef EMIT_VERTEX
}

#undef LERP
#undef DIFFERENT_SIGNS
#undef PLANE_DOT

#endif /* SW_RASTERIZE_H */
