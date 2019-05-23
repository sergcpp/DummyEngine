#include "SWculling.h"


void swCullCtxInit(SWcull_ctx *ctx, SWint w, SWint h, SWfloat zmax) {
    swZbufInit(&ctx->zbuf, w, h, zmax);
    swZbufClearDepth(&ctx->zbuf, 1.0f);
    ctx->depth_eps = (SWfloat)0.001;
}

void swCullCtxDestroy(SWcull_ctx *ctx) {
    swZbufDestroy(&ctx->zbuf);
    memset(ctx, 0, sizeof(SWcull_ctx));
}

void swCullCtxClear(SWcull_ctx *ctx) {
    swZbufClearDepth(&ctx->zbuf, ctx->zbuf.zmax);
}

void swCullCtxSubmitCullSurfs(SWcull_ctx *ctx, SWcull_surf *surfs, SWuint count) {
    for (SWuint i = 0; i < count; i++) {
        SWcull_surf *s = &surfs[i];

        if (s->dont_skip && *(s->dont_skip) == 0) continue;

        if (s->indices) {
            if (s->prim_type == SW_TRIANGLES) {
                if (s->index_type == SW_UNSIGNED_BYTE) {
                    s->visible = swCullCtxCullTrianglesIndexed_Ubyte(ctx, s->attribs, (const SWubyte *)s->indices, s->stride, s->count, s->base_vertex, s->xform, (s->type == SW_OCCLUDER));
                } else if (s->index_type == SW_UNSIGNED_INT) {
                    s->visible = swCullCtxCullTrianglesIndexed_Uint(ctx, s->attribs, (const SWuint *)s->indices, s->stride, s->count, s->base_vertex, s->xform, (s->type == SW_OCCLUDER));
                }
            }
        } else {

        }
    }
}

sw_inline void _swProcessTriangle(SWzbuffer *zbuf, const SWfloat *v0, const SWfloat *v1, const SWfloat *v2) {
    SWint p0[2], p1[2], p2[2];
    SWint min[2], max[2];
    SWfloat vs_interpolated[3];
    SWfloat vs_d10[3], vs_d20[3];

    SWint d01[2], d12[2], d20[2];
    SWint area;
    SWfloat inv_area;
    SWint C1, C2, C3;

    _swProjectOnScreen(zbuf, v0, p0);
    _swProjectOnScreen(zbuf, v1, p1);
    _swProjectOnScreen(zbuf, v2, p2);

    min[0] = sw_min(p0[0], sw_min(p1[0], p2[0]));
    min[1] = sw_min(p0[1], sw_min(p1[1], p2[1]));
    max[0] = sw_max(p0[0], sw_max(p1[0], p2[0]));
    max[1] = sw_max(p0[1], sw_max(p1[1], p2[1]));

    if (min[0] >= zbuf->w || min[1] >= zbuf->h || max[0] < 0 || max[1] < 0) return;

    if (min[0] < 0) min[0] = 0;
    if (min[1] < 0) min[1] = 0;
    if (max[0] >= zbuf->w) max[0] = zbuf->w - 1;
    if (max[1] >= zbuf->h) max[1] = zbuf->h - 1;

    min[0] &= ~(SW_TILE_SIZE - 1);
    min[1] &= ~(SW_TILE_SIZE - 1);

    d01[0] = p0[0] - p1[0];
    d01[1] = p0[1] - p1[1];
    d12[0] = p1[0] - p2[0];
    d12[1] = p1[1] - p2[1];
    d20[0] = p2[0] - p0[0];
    d20[1] = p2[1] - p0[1];

    area = d01[0] * d20[1] - d20[0] * d01[1];

    if (area <= 0) return;

    inv_area = ((SWfloat)1) / area;

    C1 = d01[1] * p0[0] - d01[0] * p0[1];
    C2 = d12[1] * p1[0] - d12[0] * p1[1];
    C3 = d20[1] * p2[0] - d20[0] * p2[1];

    if (d01[1] < 0 || (d01[1] == 0 && d01[0] > 0)) C1++;
    if (d12[1] < 0 || (d12[1] == 0 && d12[0] > 0)) C2++;
    if (d20[1] < 0 || (d20[1] == 0 && d20[0] > 0)) C3++;

    for (SWint i = 0; i < 3; i++) {
        vs_d10[i] = v1[i] - v0[i];
        vs_d20[i] = v2[i] - v0[i];
    }

    for (SWint y = min[1]; y < max[1]; y += SW_TILE_SIZE) {
        for (SWint x = min[0]; x < max[0]; x += SW_TILE_SIZE) {
            SWint x0 = x,
                  x1 = sw_min(x + SW_TILE_SIZE - 1, zbuf->w - 1),
                  y0 = y,
                  y1 = sw_min(y + SW_TILE_SIZE - 1, zbuf->h - 1);

            SWint full_tile = (x1 - x0 == SW_TILE_SIZE - 1) && (y1 - y0 == SW_TILE_SIZE - 1);

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

            SWint a = ((Cy1 > 0) << 0) | ((Cy1_10 > 0) << 1) | ((Cy1_01 > 0) << 2) | ((Cy1_11 > 0) << 3);
            SWint b = ((Cy2 > 0) << 0) | ((Cy2_10 > 0) << 1) | ((Cy2_01 > 0) << 2) | ((Cy2_11 > 0) << 3);
            SWint c = ((Cy3 > 0) << 0) | ((Cy3_10 > 0) << 1) | ((Cy3_01 > 0) << 2) | ((Cy3_11 > 0) << 3);

            if (a == 0 || b == 0 || c == 0) continue;

            SWint full_cover = (a == 15 && b == 15 && c == 15) && full_tile;

            /////

            SWfloat f00_z = v0[2] + Cy3 * inv_area * vs_d10[2] + Cy1 * inv_area * vs_d20[2];
            SWfloat f10_z = v0[2] + Cy3_10 * inv_area * vs_d10[2] + Cy1_10 * inv_area * vs_d20[2];
            SWfloat f01_z = v0[2] + Cy3_01 * inv_area * vs_d10[2] + Cy1_01 * inv_area * vs_d20[2];
            SWfloat f11_z = v0[2] + Cy3_11 * inv_area * vs_d10[2] + Cy1_11 * inv_area * vs_d20[2];

            SWfloat zmin = sw_min(f00_z, sw_min(f10_z, sw_min(f01_z, f11_z)));
            SWfloat zmax = sw_max(f00_z, sw_max(f10_z, sw_max(f01_z, f11_z)));

            SWint depth_test = 1;

            SWzrange *zr = swZbufGetTileRange(zbuf, x0, y0);
            if (zmax < zr->min) {
                depth_test = 0;
            } else if (zmin > zr->max) {
                continue;
            }

            if (full_cover && depth_test == 0) {
                swZbufSetTileRange(zbuf, x0, y0, zmin, sw_min(zmax, zbuf->zmax));
            } else {
                swZbufUpdateTileRange(zbuf, x0, y0, zmin, sw_min(zmax, zbuf->zmax));
            }

            SWfloat vs_dx[3], vs_dy[3];
            for (SWint i = 0; i < 3; i++) {
                vs_dx[i] = -d20[1] * inv_area * vs_d10[i] - d01[1] * inv_area * vs_d20[i];
                vs_dy[i] = d20[0] * inv_area * vs_d10[i] + d01[0] * inv_area * vs_d20[i] - SW_TILE_SIZE * vs_dx[i];
                vs_interpolated[i] = v0[i] + Cy3 * inv_area * vs_d10[i] + Cy1 * inv_area * vs_d20[i];
            }

            /////

            if (full_cover) {
                SWint ix, iy;
                for (iy = y; iy < y + SW_TILE_SIZE; iy++) {
                    SWint Cx1 = Cy1, Cx3 = Cy3;
                    for (ix = x; ix < x + SW_TILE_SIZE; ix++) {
                        if (!depth_test || swZbufTestDepth(zbuf, ix, iy, vs_interpolated[2])) {
                            swZbufSetDepth(zbuf, ix, iy, sw_min(vs_interpolated[2], zbuf->zmax));
                        }

                        sw_add_3(vs_interpolated, vs_dx);

                        Cx1 -= d01[1];
                        Cx3 -= d20[1];
                    }

                    sw_add_3(vs_interpolated, vs_dy);

                    Cy1 += d01[0];
                    Cy3 += d20[0];
                }
            } else {
                SWint ix, iy;
                for (iy = y; iy <= y1; iy++) {
                    SWint Cx1 = Cy1, Cx2 = Cy2, Cx3 = Cy3;
                    for (ix = x; ix <= x1; ix++) {
                        if (Cx1 > 0 && Cx2 > 0 && Cx3 > 0) {
                            if (!depth_test || swZbufTestDepth(zbuf, ix, iy, vs_interpolated[2])) {
                                swZbufSetDepth(zbuf, ix, iy, sw_min(vs_interpolated[2], zbuf->zmax));
                            }
                        }

                        sw_add_3(vs_interpolated, vs_dx);

                        Cx1 -= d01[1];
                        Cx2 -= d12[1];
                        Cx3 -= d20[1];
                    }

                    sw_add_3(vs_interpolated, vs_dy);

                    Cy1 += d01[0];
                    Cy2 += d12[0];
                    Cy3 += d20[0];
                }
            }
        }
    }
}

sw_inline SWint _swTestTriangle(const SWzbuffer *zbuf, const SWfloat *v0, const SWfloat *v1, const SWfloat *v2, SWfloat eps) {
    SWint p0[2], p1[2], p2[2];
    SWint min[2], max[2];
    SWfloat vs_interpolated[3];
    SWfloat vs_d10[3], vs_d20[3];

    SWint d01[2], d12[2], d20[2];
    SWint area;
    SWfloat inv_area;
    SWint C1, C2, C3;

    _swProjectOnScreen(zbuf, v0, p0);
    _swProjectOnScreen(zbuf, v1, p1);
    _swProjectOnScreen(zbuf, v2, p2);

    min[0] = sw_min(p0[0], sw_min(p1[0], p2[0]));
    min[1] = sw_min(p0[1], sw_min(p1[1], p2[1]));
    max[0] = sw_max(p0[0], sw_max(p1[0], p2[0]));
    max[1] = sw_max(p0[1], sw_max(p1[1], p2[1]));

    if (min[0] >= zbuf->w || min[1] >= zbuf->h || max[0] < 0 || max[1] < 0) return 0;

    if (min[0] < 0) min[0] = 0;
    if (min[1] < 0) min[1] = 0;
    if (max[0] >= zbuf->w) max[0] = zbuf->w - 1;
    if (max[1] >= zbuf->h) max[1] = zbuf->h - 1;

    min[0] &= ~(SW_TILE_SIZE - 1);
    min[1] &= ~(SW_TILE_SIZE - 1);

    d01[0] = p0[0] - p1[0];
    d01[1] = p0[1] - p1[1];
    d12[0] = p1[0] - p2[0];
    d12[1] = p1[1] - p2[1];
    d20[0] = p2[0] - p0[0];
    d20[1] = p2[1] - p0[1];

    area = d01[0] * d20[1] - d20[0] * d01[1];

    if (area <= 0) return 0;

    inv_area = ((SWfloat)1) / area;

    C1 = d01[1] * p0[0] - d01[0] * p0[1];
    C2 = d12[1] * p1[0] - d12[0] * p1[1];
    C3 = d20[1] * p2[0] - d20[0] * p2[1];

    if (d01[1] < 0 || (d01[1] == 0 && d01[0] > 0)) C1++;
    if (d12[1] < 0 || (d12[1] == 0 && d12[0] > 0)) C2++;
    if (d20[1] < 0 || (d20[1] == 0 && d20[0] > 0)) C3++;

    for (SWint i = 0; i < 3; i++) {
        vs_d10[i] = v1[i] - v0[i];
        vs_d20[i] = v2[i] - v0[i];
    }

    for (SWint y = min[1]; y < max[1]; y += SW_TILE_SIZE) {
        for (SWint x = min[0]; x < max[0]; x += SW_TILE_SIZE) {
            SWint x0 = x,
                  x1 = sw_min(x + SW_TILE_SIZE - 1, zbuf->w - 1),
                  y0 = y,
                  y1 = sw_min(y + SW_TILE_SIZE - 1, zbuf->h - 1);

            SWint full_tile = (x1 - x0 == SW_TILE_SIZE - 1) && (y1 - y0 == SW_TILE_SIZE - 1);

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

            SWint a = ((Cy1 > 0) << 0) | ((Cy1_10 > 0) << 1) | ((Cy1_01 > 0) << 2) | ((Cy1_11 > 0) << 3);
            SWint b = ((Cy2 > 0) << 0) | ((Cy2_10 > 0) << 1) | ((Cy2_01 > 0) << 2) | ((Cy2_11 > 0) << 3);
            SWint c = ((Cy3 > 0) << 0) | ((Cy3_10 > 0) << 1) | ((Cy3_01 > 0) << 2) | ((Cy3_11 > 0) << 3);

            if (a == 0 || b == 0 || c == 0) continue;

            SWint full_cover = (a == 15 && b == 15 && c == 15) && full_tile;

            /////

            SWfloat f00_z = v0[2] + Cy3 * inv_area * vs_d10[2] + Cy1 * inv_area * vs_d20[2];
            SWfloat f10_z = v0[2] + Cy3_10 * inv_area * vs_d10[2] + Cy1_10 * inv_area * vs_d20[2];
            SWfloat f01_z = v0[2] + Cy3_01 * inv_area * vs_d10[2] + Cy1_01 * inv_area * vs_d20[2];
            SWfloat f11_z = v0[2] + Cy3_11 * inv_area * vs_d10[2] + Cy1_11 * inv_area * vs_d20[2];

            SWfloat zmin = sw_min(f00_z, sw_min(f10_z, sw_min(f01_z, f11_z)));
            SWfloat zmax = sw_max(f00_z, sw_max(f10_z, sw_max(f01_z, f11_z)));

            const SWzrange *zr = swZbufGetTileRange(zbuf, x0, y0);
            if (zmax < zr->min) {
                return 1;
            } else if (zmin > zr->max && zr->max != zbuf->zmax) {
                continue;
            }

            SWfloat vs_dx[3], vs_dy[3];
            for (SWint i = 0; i < 3; i++) {
                vs_dx[i] = -d20[1] * inv_area * vs_d10[i] - d01[1] * inv_area * vs_d20[i];
                vs_dy[i] = d20[0] * inv_area * vs_d10[i] + d01[0] * inv_area * vs_d20[i] - SW_TILE_SIZE * vs_dx[i];
                vs_interpolated[i] = v0[i] + Cy3 * inv_area * vs_d10[i] + Cy1 * inv_area * vs_d20[i];
            }

            /////

            if (full_cover) {
                SWint ix, iy;
                for (iy = y; iy < y + SW_TILE_SIZE; iy++) {
                    SWint Cx1 = Cy1, Cx3 = Cy3;
                    for (ix = x; ix < x + SW_TILE_SIZE; ix++) {
                        SWfloat z = swZbufGetDepth(zbuf, ix, iy);
                        if (z == zbuf->zmax || (z - vs_interpolated[2]) > -eps) {
                            return 1;
                        }

                        sw_add_3(vs_interpolated, vs_dx);

                        Cx1 -= d01[1];
                        Cx3 -= d20[1];
                    }

                    sw_add_3(vs_interpolated, vs_dy);

                    Cy1 += d01[0];
                    Cy3 += d20[0];
                }
            } else {
                SWint ix, iy;
                for (iy = y; iy <= y1; iy++) {
                    SWint Cx1 = Cy1, Cx2 = Cy2, Cx3 = Cy3;
                    for (ix = x; ix <= x1; ix++) {
                        if (Cx1 > 0 && Cx2 > 0 && Cx3 > 0) {
                            SWfloat z = swZbufGetDepth(zbuf, ix, iy);
                            if (z == zbuf->zmax || (z - vs_interpolated[2]) > -eps) {
                                return 1;
                            }
                        }

                        sw_add_3(vs_interpolated, vs_dx);

                        Cx1 -= d01[1];
                        Cx2 -= d12[1];
                        Cx3 -= d20[1];
                    }

                    sw_add_3(vs_interpolated, vs_dy);

                    Cy1 += d01[0];
                    Cy2 += d12[0];
                    Cy3 += d20[0];
                }
            }
        }
    }

    return 0;
}

SWint swCullCtxCullTrianglesIndexed_Ubyte(SWcull_ctx *ctx, const void *attribs, const SWubyte *indices,
        SWuint stride, SWuint count, SWint base_vertex, const SWfloat *xform, SWint is_occluder) {
    SWint res = 0;
    SWfloat vs_out[16][4];

    for (SWuint j = 0; j < count; j += 3) {
        SWuint i0 = indices[j] + base_vertex;
        SWfloat v0_x = ((SWfloat *)((uintptr_t)attribs + i0 * stride))[0];
        SWfloat v0_y = ((SWfloat *)((uintptr_t)attribs + i0 * stride))[1];
        SWfloat v0_z = ((SWfloat *)((uintptr_t)attribs + i0 * stride))[2];

        SWuint i1 = indices[j + 1] + base_vertex;
        SWfloat v1_x = ((SWfloat *)((uintptr_t)attribs + i1 * stride))[0];
        SWfloat v1_y = ((SWfloat *)((uintptr_t)attribs + i1 * stride))[1];
        SWfloat v1_z = ((SWfloat *)((uintptr_t)attribs + i1 * stride))[2];

        SWuint i2 = indices[j + 2] + base_vertex;
        SWfloat v2_x = ((SWfloat *)((uintptr_t)attribs + i2 * stride))[0];
        SWfloat v2_y = ((SWfloat *)((uintptr_t)attribs + i2 * stride))[1];
        SWfloat v2_z = ((SWfloat *)((uintptr_t)attribs + i2 * stride))[2];

        vs_out[0][0] = xform[0] * v0_x + xform[4] * v0_y + xform[8] * v0_z + xform[12];
        vs_out[0][1] = xform[1] * v0_x + xform[5] * v0_y + xform[9] * v0_z + xform[13];
        vs_out[0][2] = xform[2] * v0_x + xform[6] * v0_y + xform[10] * v0_z + xform[14];
        vs_out[0][3] = xform[3] * v0_x + xform[7] * v0_y + xform[11] * v0_z + xform[15];

        vs_out[1][0] = xform[0] * v1_x + xform[4] * v1_y + xform[8] * v1_z + xform[12];
        vs_out[1][1] = xform[1] * v1_x + xform[5] * v1_y + xform[9] * v1_z + xform[13];
        vs_out[1][2] = xform[2] * v1_x + xform[6] * v1_y + xform[10] * v1_z + xform[14];
        vs_out[1][3] = xform[3] * v1_x + xform[7] * v1_y + xform[11] * v1_z + xform[15];

        vs_out[2][0] = xform[0] * v2_x + xform[4] * v2_y + xform[8] * v2_z + xform[12];
        vs_out[2][1] = xform[1] * v2_x + xform[5] * v2_y + xform[9] * v2_z + xform[13];
        vs_out[2][2] = xform[2] * v2_x + xform[6] * v2_y + xform[10] * v2_z + xform[14];
        vs_out[2][3] = xform[3] * v2_x + xform[7] * v2_y + xform[11] * v2_z + xform[15];

        SWint out_verts[16];
        SWint num_verts = _swClipAndTransformToNDC_Tri_Simple(vs_out, vs_out, 0, 1, 2, is_occluder ? ctx->zbuf.zmax : 1.0f, out_verts);

        for (SWint i = 1; i < num_verts - 1; i++) {
            if (is_occluder) {
                _swProcessTriangle(&ctx->zbuf, vs_out[out_verts[0]], vs_out[out_verts[i]], vs_out[out_verts[i + 1]]);
            } else {
                res = _swTestTriangle(&ctx->zbuf, vs_out[out_verts[0]], vs_out[out_verts[i]], vs_out[out_verts[i + 1]], ctx->depth_eps);
                if (res) return 1;
            }
        }
    }

    return res;
}

SWint swCullCtxCullTrianglesIndexed_Uint(SWcull_ctx *ctx, const void *attribs, const SWuint *indices,
        SWuint stride, SWuint count, SWint base_vertex, const SWfloat *xform, SWint is_occluder) {
    SWint res = is_occluder;
    SWfloat vs_out[16][4];

    for (SWuint j = 0; j < count; j += 3) {
        SWuint i0 = indices[j] + base_vertex;
        SWfloat v0_x = ((SWfloat *)((uintptr_t)attribs + i0 * stride))[0];
        SWfloat v0_y = ((SWfloat *)((uintptr_t)attribs + i0 * stride))[1];
        SWfloat v0_z = ((SWfloat *)((uintptr_t)attribs + i0 * stride))[2];

        SWuint i1 = indices[j + 1] + base_vertex;
        SWfloat v1_x = ((SWfloat *)((uintptr_t)attribs + i1 * stride))[0];
        SWfloat v1_y = ((SWfloat *)((uintptr_t)attribs + i1 * stride))[1];
        SWfloat v1_z = ((SWfloat *)((uintptr_t)attribs + i1 * stride))[2];

        SWuint i2 = indices[j + 2] + base_vertex;
        SWfloat v2_x = ((SWfloat *)((uintptr_t)attribs + i2 * stride))[0];
        SWfloat v2_y = ((SWfloat *)((uintptr_t)attribs + i2 * stride))[1];
        SWfloat v2_z = ((SWfloat *)((uintptr_t)attribs + i2 * stride))[2];

        vs_out[0][0] = xform[0] * v0_x + xform[4] * v0_y + xform[8] * v0_z + xform[12];
        vs_out[0][1] = xform[1] * v0_x + xform[5] * v0_y + xform[9] * v0_z + xform[13];
        vs_out[0][2] = xform[2] * v0_x + xform[6] * v0_y + xform[10] * v0_z + xform[14];
        vs_out[0][3] = xform[3] * v0_x + xform[7] * v0_y + xform[11] * v0_z + xform[15];

        vs_out[1][0] = xform[0] * v1_x + xform[4] * v1_y + xform[8] * v1_z + xform[12];
        vs_out[1][1] = xform[1] * v1_x + xform[5] * v1_y + xform[9] * v1_z + xform[13];
        vs_out[1][2] = xform[2] * v1_x + xform[6] * v1_y + xform[10] * v1_z + xform[14];
        vs_out[1][3] = xform[3] * v1_x + xform[7] * v1_y + xform[11] * v1_z + xform[15];

        vs_out[2][0] = xform[0] * v2_x + xform[4] * v2_y + xform[8] * v2_z + xform[12];
        vs_out[2][1] = xform[1] * v2_x + xform[5] * v2_y + xform[9] * v2_z + xform[13];
        vs_out[2][2] = xform[2] * v2_x + xform[6] * v2_y + xform[10] * v2_z + xform[14];
        vs_out[2][3] = xform[3] * v2_x + xform[7] * v2_y + xform[11] * v2_z + xform[15];

        SWint out_verts[16];
        SWint num_verts = _swClipAndTransformToNDC_Tri_Simple(vs_out, vs_out, 0, 1, 2, is_occluder ? ctx->zbuf.zmax : 1.0f, out_verts);

        for (SWint i = 1; i < num_verts - 1; i++) {
            if (is_occluder) {
                _swProcessTriangle(&ctx->zbuf, vs_out[out_verts[0]], vs_out[out_verts[i]], vs_out[out_verts[i + 1]]);
            } else {
                res = _swTestTriangle(&ctx->zbuf, vs_out[out_verts[0]], vs_out[out_verts[i]], vs_out[out_verts[i + 1]], ctx->depth_eps);
                if (res) return 1;
            }
        }
    }

    return res;
}