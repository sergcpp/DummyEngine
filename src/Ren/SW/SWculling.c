#include "SWculling.h"

#include "SWalloc.h"
#include "SWintrin.inl"

void _swProcessTrianglesIndexed_Ref(SWcull_ctx *ctx, const void *attribs,
                                    const SWuint *indices, const SWuint stride,
                                    const SWuint index_count, const SWfloat *xform);

#ifndef __ANDROID__
void _swProcessTrianglesIndexed_SSE2(SWcull_ctx *ctx, const void *attribs,
                                     const SWuint *indices, const SWuint stride,
                                     const SWuint index_count, const SWfloat *xform);
void _swProcessTrianglesIndexed_AVX2(SWcull_ctx *ctx, const void *attribs,
                                     const SWuint *indices, const SWuint stride,
                                     const SWuint index_count, const SWfloat *xform);
void _swProcessTrianglesIndexed_AVX512(SWcull_ctx *ctx, const void *attribs,
                                       const SWuint *indices, const SWuint stride,
                                       const SWuint index_count, const SWfloat *xform);
#endif

void swCullCtxInit(SWcull_ctx *ctx, const SWint w, const SWint h, const SWfloat zmax) {
    swCPUInfoInit(&ctx->cpu_info);
    swZbufInit(&ctx->zbuf, w, h, zmax);

    ctx->half_w = (SWfloat)w / 2.0f;
    ctx->half_h = (SWfloat)h / 2.0f;

#ifndef __ANDROID__
    if (ctx->cpu_info.avx512_supported) {
        ctx->tile_size_y = 16;
        ctx->tri_indexed_proc =
            (TrianglesIndexedProcType)&_swProcessTrianglesIndexed_AVX512;
    } else if (ctx->cpu_info.avx2_supported) {
        ctx->tile_size_y = 8;
        ctx->tri_indexed_proc =
            (TrianglesIndexedProcType)&_swProcessTrianglesIndexed_AVX2;
    } else if (ctx->cpu_info.sse2_supported) {
        ctx->tile_size_y = 4;
        ctx->tri_indexed_proc =
            (TrianglesIndexedProcType)&_swProcessTrianglesIndexed_SSE2;
    } else
#endif
    {
        ctx->tile_size_y = 1;
        ctx->tri_indexed_proc = (TrianglesIndexedProcType)&_swProcessTrianglesIndexed_Ref;
    }

    ctx->cov_tile_w = (w + (SW_CULL_TILE_SIZE_X - 1)) / SW_CULL_TILE_SIZE_X;
    ctx->cov_tile_h = (h + (ctx->tile_size_y - 1)) / ctx->tile_size_y;
    ctx->coverage =
        (uint32_t *)aligned_malloc(sizeof(uint32_t) * ctx->cov_tile_w * h, 64);
    memset(ctx->coverage, 0, (size_t)ctx->cov_tile_w * ctx->zbuf.h * sizeof(uint32_t));

    __m128 *clip_planes = (__m128 *)ctx->clip_planes;

    const SWfloat pad_w = 2.0f / ctx->zbuf.w;
    const SWfloat pad_h = 2.0f / ctx->zbuf.h;

    clip_planes[0] = _mm128_setr_ps(1.0f - pad_w, 0.0f, 1.0f, 0.0f);
    clip_planes[1] = _mm128_setr_ps(-1.0f + pad_w, 0.0f, 1.0f, 0.0f);
    clip_planes[2] = _mm128_setr_ps(0.0f, -1.0f + pad_h, 1.0f, 0.0f);
    clip_planes[3] = _mm128_setr_ps(0.0f, 1.0f - pad_h, 1.0f, 0.0f);
    clip_planes[4] = _mm128_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);

    swZbufClearDepth(&ctx->zbuf, 1.0f);
    ctx->depth_eps = (SWfloat)0.001;
}

void swCullCtxDestroy(SWcull_ctx *ctx) {
    swCPUInfoDestroy(&ctx->cpu_info);
    swZbufDestroy(&ctx->zbuf);
    aligned_free(ctx->coverage);
    memset(ctx, 0, sizeof(SWcull_ctx));
}

void swCullCtxClear(SWcull_ctx *ctx) {
    swZbufClearDepth(&ctx->zbuf, ctx->zbuf.zmax);
    memset(ctx->coverage, 0, (size_t)ctx->cov_tile_w * ctx->zbuf.h * sizeof(uint32_t));
}

void swCullCtxSubmitCullSurfs(SWcull_ctx *ctx, SWcull_surf *surfs, const SWuint count) {
    for (SWuint i = 0; i < count; i++) {
        SWcull_surf *s = &surfs[i];

        if (s->dont_skip && *(s->dont_skip) == 0)
            continue;

        if (s->indices) {
            if (s->prim_type == SW_TRIANGLES) {
                if (s->index_type == SW_UNSIGNED_BYTE) {
                    //s->visible = swCullCtxCullTrianglesIndexed_Ubyte(
                    //    ctx, s->attribs, (const SWubyte *)s->indices, s->stride, s->count,
                    //    s->base_vertex, s->xform, (s->type == SW_OCCLUDER));
                } else if (s->index_type == SW_UNSIGNED_INT) {
                    s->visible = swCullCtxCullTrianglesIndexed_Uint(
                        ctx, s->attribs, (const SWuint *)s->indices, s->stride, s->count,
                        s->base_vertex, s->xform, (s->type == SW_OCCLUDER));
                }
            }
        } else {
        }
    }
}

sw_inline SWint _swTestTriangle(const SWcull_ctx *ctx, const SWfloat *v0,
                                const SWfloat *v1, const SWfloat *v2, SWfloat eps) {
    const SWzbuffer *zbuf = &ctx->zbuf;

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

    if (min[0] >= zbuf->w || min[1] >= zbuf->h || max[0] < 0 || max[1] < 0)
        return 0;

    if (min[0] < 0)
        min[0] = 0;
    if (min[1] < 0)
        min[1] = 0;
    if (max[0] >= zbuf->w)
        max[0] = zbuf->w - 1;
    if (max[1] >= zbuf->h)
        max[1] = zbuf->h - 1;

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
        return 0;

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

    for (SWint i = 0; i < 3; i++) {
        vs_d10[i] = v1[i] - v0[i];
        vs_d20[i] = v2[i] - v0[i];
    }

    for (SWint y = min[1]; y < max[1]; y += SW_TILE_SIZE) {
        for (SWint x = min[0]; x < max[0]; x += SW_TILE_SIZE) {
            SWint x0 = x, x1 = sw_min(x + SW_TILE_SIZE - 1, zbuf->w - 1), y0 = y,
                  y1 = sw_min(y + SW_TILE_SIZE - 1, zbuf->h - 1);

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
                v0[2] + Cy3 * inv_area * vs_d10[2] + Cy1 * inv_area * vs_d20[2];
            SWfloat f10_z =
                v0[2] + Cy3_10 * inv_area * vs_d10[2] + Cy1_10 * inv_area * vs_d20[2];
            SWfloat f01_z =
                v0[2] + Cy3_01 * inv_area * vs_d10[2] + Cy1_01 * inv_area * vs_d20[2];
            SWfloat f11_z =
                v0[2] + Cy3_11 * inv_area * vs_d10[2] + Cy1_11 * inv_area * vs_d20[2];

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
                vs_dy[i] = d20[0] * inv_area * vs_d10[i] + d01[0] * inv_area * vs_d20[i] -
                           SW_TILE_SIZE * vs_dx[i];
                vs_interpolated[i] =
                    v0[i] + Cy3 * inv_area * vs_d10[i] + Cy1 * inv_area * vs_d20[i];
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

SWint swCullCtxCullTrianglesIndexed_Uint(SWcull_ctx *ctx, const void *attribs,
                                         const SWuint *indices, const SWuint stride,
                                         const SWuint index_count,
                                         const SWint base_vertex, const SWfloat *xform,
                                         const SWint is_occluder) {
    SWint res = is_occluder;

    // Apply atrribute padding
    attribs = (const void *)((uintptr_t)attribs + stride * base_vertex);

    ctx->tri_indexed_proc(ctx, attribs, indices, stride, index_count, xform);

    return res;
}

SWint _swClipPolygon(const __m128 in_vtx[], const SWint in_vtx_count, const __m128 plane,
                     __m128 out_vtx[]) {
    __m128 p0 = in_vtx[in_vtx_count - 1];
    __m128 dist0 = _mm128_dp4_ps(plane, p0);

    SWint out_vtx_count = 0;
    for (SWint i = 0; i < in_vtx_count; i++) {
        const __m128 p1 = in_vtx[i];
        const __m128 dist1 = _mm128_dp4_ps(plane, p1);
        const int dist0_neg = _mm128_movemask_ps(dist0);
        if (!dist0_neg) { // dist0 > 0.0f
            out_vtx[out_vtx_count++] = p0;
        }

        // if dist0 and dist1 have different signs (segment intersects plane)
        if (_mm128_movemask_ps(_mm128_xor_ps(dist0, dist1))) {
            if (!dist0_neg) {
                const __m128 t = _mm128_div_ps(dist0, _mm128_sub_ps(dist0, dist1));
                out_vtx[out_vtx_count++] = _mm128_fmadd_ps(_mm128_sub_ps(p1, p0), t, p0);
            } else {
                const __m128 t = _mm128_div_ps(dist1, _mm128_sub_ps(dist1, dist0));
                out_vtx[out_vtx_count++] = _mm128_fmadd_ps(_mm128_sub_ps(p0, p1), t, p1);
            }
        }

        dist0 = dist1;
        p0 = p1;
    }

    return out_vtx_count;
}

void swCullCtxTestCoverage(SWcull_ctx *ctx) {
    /*for (SWint j = 0; j < ctx->zbuf.h; j++) {
        SWint cj = j / SW_CULL_TILE_SIZE_Y;
        for (SWint i = 0; i < ctx->zbuf.w; i++) {
            SWint ci = i / SW_CULL_TILE_SIZE_X;

            SWfloat* depth_val = &ctx->zbuf.depth[j * ctx->zbuf.w + i];

            uint32_t mask = ctx->zbuf.coverage[cj * ctx->zbuf.cov_tile_w + ci];
            if (mask & (1 << (i % SW_CULL_TILE_SIZE_X))) {
                (*depth_val) = 1.0f;
            } else {
                (*depth_val) = 0.0f;
            }
        }
    }*/

    for (SWint y = 0; y < ctx->zbuf.h; y++) {
        SWint ty = y / ctx->tile_size_y;
        for (SWint x = 0; x < ctx->zbuf.w; x++) {
            SWint tx = x / SW_CULL_TILE_SIZE_X;

            SWint tile_ndx = ty * ctx->cov_tile_w + tx;

#if 0 // in case it is transposed (needed later)
            SWint stx = (x % SW_CULL_TILE_SIZE_X) / 8;
            SWint sty = (y % SW_CULL_TILE_SIZE_Y) / 4;
            SWint subtile_ndx = sty * 4 + stx;

            SWint px = (x % 8);
            SWint py = (y % 4);
            SWint bit_ndx = py * 8 + px;

            SWint pix = (ctx->zbuf.coverage[tile_ndx * SW_CULL_TILE_SIZE_Y + subtile_ndx] >> bit_ndx) & 1;
#else
            SWint subtile_ndx = (y % ctx->tile_size_y);
            SWint bit_ndx = (x % SW_CULL_TILE_SIZE_X);

            SWint pix =
                (ctx->coverage[tile_ndx * ctx->tile_size_y + subtile_ndx] >> bit_ndx) & 1;
#endif

            SWfloat *depth_val = &ctx->zbuf.depth[y * ctx->zbuf.w + x];
            if (pix) {
                (*depth_val) = 1.0f;
            } else {
                (*depth_val) = 0.0f;
            }
        }
    }

    /*for (SWint j = 0; j < ctx->zbuf.cov_tile_h; j++) {
        for (SWint i = 0; i < ctx->zbuf.cov_tile_w * 4; i++) {
            uint32_t *tile = &ctx->zbuf.coverage[j * ctx->zbuf.cov_tile_w * 4 + i];

            for (SWint jj = 0; jj < SW_CULL_TILE_SIZE_Y; jj++) {
                for (SWint ii = 0; ii < SW_CULL_TILE_SIZE_X; ii++) {
                    SWint px_index = (j * SW_CULL_TILE_SIZE_Y + jj) * ctx->zbuf.w + (i *
    SW_CULL_TILE_SIZE_X + ii);

                    SWfloat* depth_val = &ctx->zbuf.depth[px_index];

                    uint32_t mask = tile[jj];
                    if (mask & (1 << ii)) {
                        (*depth_val) = 1.0f;
                    } else {
                        (*depth_val) = 0.0f;
                    }
                }
            }
        }
    }*/
}