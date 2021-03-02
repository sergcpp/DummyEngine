#include "SWintrin.inl"

#define FP_BITS (19 - SW_CULL_TILE_HEIGHT_SHIFT)

#define PASTER(x, y) x##_##y
#define EVALUATOR(x, y) PASTER(x, y)
#define NAME(fun) EVALUATOR(fun, POSTFIX)

//
// Scanline processing
//

#define SCANLINE_FUNC_NAME NAME(_swProcessScanline_L1R1)
#define LEFT_COUNT 1
#define RIGHT_COUNT 1
#include "SWculling_rast_scanline.inl"
#undef RIGHT_COUNT
#undef LEFT_COUNT
#undef SCANLINE_FUNC_NAME

#define SCANLINE_FUNC_NAME NAME(_swProcessScanline_L2R1)
#define LEFT_COUNT 2
#define RIGHT_COUNT 1
#include "SWculling_rast_scanline.inl"
#undef RIGHT_COUNT
#undef LEFT_COUNT
#undef SCANLINE_FUNC_NAME

#define SCANLINE_FUNC_NAME NAME(_swProcessScanline_L1R2)
#define LEFT_COUNT 1
#define RIGHT_COUNT 2
#include "SWculling_rast_scanline.inl"
#undef RIGHT_COUNT
#undef LEFT_COUNT
#undef SCANLINE_FUNC_NAME

//
// Triangle processing
//

#define TRI_FUNC_NAME NAME(_swRasterizeTriangle_tight_mid_left)
#define TIGHT_TRAVERSAL
#define MID_VTX_RIGHT 0
#include "SWculling_rast_tri.inl"
#undef MID_VTX_RIGHT
#undef TIGHT_TRAVERSAL
#undef TRI_FUNC_NAME

#define TRI_FUNC_NAME NAME(_swRasterizeTriangle_tight_mid_right)
#define TIGHT_TRAVERSAL
#define MID_VTX_RIGHT 1
#include "SWculling_rast_tri.inl"
#undef MID_VTX_RIGHT
#undef TIGHT_TRAVERSAL
#undef TRI_FUNC_NAME

#define MID_VTX_RIGHT 0
#define TRI_FUNC_NAME NAME(_swRasterizeTriangle_mid_left)
#include "SWculling_rast_tri.inl"
#undef MID_VTX_RIGHT
#undef TRI_FUNC_NAME

#define MID_VTX_RIGHT 1
#define TRI_FUNC_NAME NAME(_swRasterizeTriangle_mid_right)
#include "SWculling_rast_tri.inl"
#undef MID_VTX_RIGHT
#undef TRI_FUNC_NAME

// bit scan forward
static inline long _swGetFirstBit(long mask) {
#ifdef _MSC_VER
    unsigned long ret;
    _BitScanForward(&ret, (unsigned long)mask);
    return (long)ret;
#else
    return (long)(__builtin_ffsl(mask) - 1);
#endif
}

void NAME(_swProcessTriangleBatch)(SWcull_ctx *ctx, __mXXX vX[3], __mXXX vY[3],
                                   __mXXX vZ[3], SWuint tri_mask) {
    // find triangle bounds
    __mXXXi bb_px_min_x =
        _mmXXX_cvttps_epi32(_mmXXX_min_ps(vX[0], _mmXXX_min_ps(vX[1], vX[2])));
    __mXXXi bb_px_min_y =
        _mmXXX_cvttps_epi32(_mmXXX_min_ps(vY[0], _mmXXX_min_ps(vY[1], vY[2])));
    __mXXXi bb_px_max_x =
        _mmXXX_cvttps_epi32(_mmXXX_max_ps(vX[0], _mmXXX_max_ps(vX[1], vX[2])));
    __mXXXi bb_px_max_y =
        _mmXXX_cvttps_epi32(_mmXXX_max_ps(vY[0], _mmXXX_max_ps(vY[1], vY[2])));

    // clip to frame bounds
    bb_px_min_x = _mmXXX_max_epi32(bb_px_min_x, _mmXXX_set1_epi32(0));
    bb_px_max_x = _mmXXX_min_epi32(bb_px_max_x, _mmXXX_set1_epi32(ctx->zbuf.w - 1));
    bb_px_min_y = _mmXXX_max_epi32(bb_px_min_y, _mmXXX_set1_epi32(0));
    bb_px_max_y = _mmXXX_min_epi32(bb_px_max_y, _mmXXX_set1_epi32(ctx->zbuf.h - 1));

    // snap to tile bounds (min % TILE_SIZE_, (max + TILE_SIZE_ - 1) % TILE_SIZE_)
    bb_px_min_x =
        _mmXXX_and_siXXX(bb_px_min_x, _mmXXX_set1_epi32(~(SW_CULL_TILE_SIZE_X - 1)));
    bb_px_max_x = _mmXXX_and_siXXX(
        _mmXXX_add_epi32(bb_px_max_x, _mmXXX_set1_epi32(SW_CULL_TILE_SIZE_X - 1)),
        _mmXXX_set1_epi32(~(SW_CULL_TILE_SIZE_X - 1)));
    bb_px_min_y =
        _mmXXX_and_siXXX(bb_px_min_y, _mmXXX_set1_epi32(~(SW_CULL_TILE_SIZE_Y - 1)));
    bb_px_max_y = _mmXXX_and_siXXX(
        _mmXXX_add_epi32(bb_px_max_y, _mmXXX_set1_epi32(SW_CULL_TILE_SIZE_Y - 1)),
        _mmXXX_set1_epi32(~(SW_CULL_TILE_SIZE_Y - 1)));

    // tiled bounding box (already padded)
    const __mXXXi bb_tile_min_x =
        _mmXXX_srai_epi32(bb_px_min_x, SW_CULL_TILE_WIDTH_SHIFT);
    const __mXXXi bb_tile_min_y =
        _mmXXX_srai_epi32(bb_px_min_y, SW_CULL_TILE_HEIGHT_SHIFT);
    const __mXXXi bb_tile_max_x =
        _mmXXX_srai_epi32(bb_px_max_x, SW_CULL_TILE_WIDTH_SHIFT);
    const __mXXXi bb_tile_max_y =
        _mmXXX_srai_epi32(bb_px_max_y, SW_CULL_TILE_HEIGHT_SHIFT);

    union {
        __mXXXi vec;
        SWint i32[SIMD_WIDTH];
    } bb_tile_size_x, bb_tile_size_y, bb_bottom_ndx, bb_top_ndx, bb_mid_ndx,
        slope_tile_delta[3], event_start[3], slope_fp[3];
    bb_tile_size_x.vec = _mmXXX_sub_epi32(bb_tile_max_x, bb_tile_min_x);
    bb_tile_size_y.vec = _mmXXX_sub_epi32(bb_tile_max_y, bb_tile_min_y);
    // Cull triangles with zero bounding box
    __mXXXi bbox_sign =
        _mmXXX_or_siXXX(_mmXXX_sub_epi32(bb_tile_size_x.vec, _mmXXX_set1_epi32(1)),
                        _mmXXX_sub_epi32(bb_tile_size_y.vec, _mmXXX_set1_epi32(1)));
    tri_mask &=
        ~_mmXXX_movemask_ps(_mmXXX_castsiXXX_ps(bbox_sign)) & ((1 << SIMD_WIDTH) - 1);
    if (!tri_mask) {
        // skip whole batch
        return;
    }

    // Rotate vertices in winding order until p0 ends up at the bottom
    for (SWint i = 0; i < 2; i++) {
        const __mXXX ey1 = _mmXXX_sub_ps(vY[1], vY[0]);
        const __mXXX ey2 = _mmXXX_sub_ps(vY[2], vY[0]);
        // ey1 < 0 || ey2 < 0
        const __mXXX mask1 = _mmXXX_or_ps(ey1, ey2);
        // ey2 == 0
        const __mXXX mask2 = _mmXXX_castsiXXX_ps(
            _mmXXX_cmpeq_epi32(_mmXXX_castps_siXXX(ey2), _mmXXX_setzero_siXXX()));
        // ey1 < 0 || ey2 <= 0
        const __mXXX swap_mask = _mmXXX_or_ps(mask1, mask2);
        const __mXXX tmp_x = _mmXXX_blendv_ps(vX[2], vX[0], swap_mask);
        vX[0] = _mmXXX_blendv_ps(vX[0], vX[1], swap_mask);
        vX[1] = _mmXXX_blendv_ps(vX[1], vX[2], swap_mask);
        vX[2] = tmp_x;
        const __mXXX tmp_y = _mmXXX_blendv_ps(vY[2], vY[0], swap_mask);
        vY[0] = _mmXXX_blendv_ps(vY[0], vY[1], swap_mask);
        vY[1] = _mmXXX_blendv_ps(vY[1], vY[2], swap_mask);
        vY[2] = tmp_y;
    }

    // Compute edges
    const __mXXX edge_x[3] = {_mmXXX_sub_ps(vX[1], vX[0]), _mmXXX_sub_ps(vX[2], vX[1]),
                              _mmXXX_sub_ps(vX[2], vX[0])};
    const __mXXX edge_y[3] = {_mmXXX_sub_ps(vY[1], vY[0]), _mmXXX_sub_ps(vY[2], vY[1]),
                              _mmXXX_sub_ps(vY[2], vY[0])};

    // Classify if the middle vertex is on the left or right and compute its position
    const SWint mid_vtx_right = ~_mmXXX_movemask_ps(edge_y[1]);
    const __mXXX mid_px_x = _mmXXX_blendv_ps(vX[1], vX[2], edge_y[1]);
    const __mXXX mid_px_y = _mmXXX_blendv_ps(vY[1], vY[2], edge_y[1]);
    const __mXXXi mid_px_yi = _mmXXX_cvttps_epi32(mid_px_y);
    const __mXXXi mid_tile_y = _mmXXX_srai_epi32(
        _mmXXX_max_epi32(mid_px_yi, _mmXXX_setzero_siXXX()), SW_CULL_TILE_HEIGHT_SHIFT);
    const __mXXXi bb_mid_tile_y =
        _mmXXX_max_epi32(bb_tile_min_y, _mmXXX_min_epi32(bb_tile_max_y, mid_tile_y));

    const SWint flat_bottom = _mmXXX_movemask_ps(
        _mmXXX_castsiXXX_ps(_mmXXX_cmpeq_epi32(bb_px_min_y, mid_px_yi)));
    __mXXX slope[3] = {_mmXXX_div_ps(edge_x[0], edge_y[0]),
                       _mmXXX_div_ps(edge_x[1], edge_y[1]),
                       _mmXXX_div_ps(edge_x[2], edge_y[2])};

    const __mXXX horizontal_slope_delta =
        _mmXXX_set1_ps((SWfloat)ctx->zbuf.w + 2.0f * (1.0f + 1.0f));
    slope[0] = _mmXXX_blendv_ps(slope[0], horizontal_slope_delta,
                                _mmXXX_cmpeq_ps(edge_y[0], _mmXXX_setzero_ps()));
    slope[1] = _mmXXX_blendv_ps(slope[1], _mmXXX_neg_ps(horizontal_slope_delta),
                                _mmXXX_cmpeq_ps(edge_y[1], _mmXXX_setzero_ps()));

    // Convert floaing point slopes to fixed point
    slope_fp[0].vec =
        _mmXXX_cvttps_epi32(_mmXXX_mul_ps(slope[0], _mmXXX_set1_ps(1 << FP_BITS)));
    slope_fp[1].vec =
        _mmXXX_cvttps_epi32(_mmXXX_mul_ps(slope[1], _mmXXX_set1_ps(1 << FP_BITS)));
    slope_fp[2].vec =
        _mmXXX_cvttps_epi32(_mmXXX_mul_ps(slope[2], _mmXXX_set1_ps(1 << FP_BITS)));

    // bias to avoid cracks
    slope_fp[0].vec =
        _mmXXX_add_epi32(slope_fp[0].vec, _mmXXX_set1_epi32(1)); // always up
    slope_fp[1].vec = _mmXXX_add_epi32(
        slope_fp[1].vec,
        _mmXXX_srli_epi32(_mmXXX_not_siXXX(_mmXXX_castps_siXXX(edge_y[1])),
                          31)); // left or right

    // Compute slope deltas for an SIMD_LANES scanline step (tile height)
    slope_tile_delta[0].vec =
        _mmXXX_slli_epi32(slope_fp[0].vec, SW_CULL_TILE_HEIGHT_SHIFT);
    slope_tile_delta[1].vec =
        _mmXXX_slli_epi32(slope_fp[1].vec, SW_CULL_TILE_HEIGHT_SHIFT);
    slope_tile_delta[2].vec =
        _mmXXX_slli_epi32(slope_fp[2].vec, SW_CULL_TILE_HEIGHT_SHIFT);

    const __mXXXi x_diffi[2] = {
        _mmXXX_slli_epi32(_mmXXX_sub_epi32(_mmXXX_cvttps_epi32(vX[0]), bb_px_min_x),
                          FP_BITS),
        _mmXXX_slli_epi32(_mmXXX_sub_epi32(_mmXXX_cvttps_epi32(mid_px_x), bb_px_min_x),
                          FP_BITS)};
    const __mXXXi y_diffi[2] = {
        _mmXXX_sub_epi32(_mmXXX_cvttps_epi32(vY[0]), bb_px_min_y),
        _mmXXX_sub_epi32(_mmXXX_cvttps_epi32(mid_px_y),
                         _mmXXX_slli_epi32(bb_mid_tile_y, SW_CULL_TILE_HEIGHT_SHIFT))};
    event_start[0].vec =
        _mmXXX_sub_epi32(x_diffi[0], _mmXXX_mullo_epi32(slope_fp[0].vec, y_diffi[0]));
    event_start[1].vec =
        _mmXXX_sub_epi32(x_diffi[1], _mmXXX_mullo_epi32(slope_fp[1].vec, y_diffi[1]));
    event_start[2].vec =
        _mmXXX_sub_epi32(x_diffi[0], _mmXXX_mullo_epi32(slope_fp[2].vec, y_diffi[0]));

    bb_bottom_ndx.vec = _mmXXX_add_epi32(
        bb_tile_min_x,
        _mmXXX_mullo_epi32(bb_tile_min_y, _mmXXX_set1_epi32(ctx->cov_tile_w)));
    bb_top_ndx.vec = _mmXXX_add_epi32(
        bb_tile_min_x,
        _mmXXX_mullo_epi32(_mmXXX_add_epi32(bb_tile_min_y, bb_tile_size_y.vec),
                           _mmXXX_set1_epi32(ctx->cov_tile_w)));
    bb_mid_ndx.vec = _mmXXX_add_epi32(
        bb_tile_min_x,
        _mmXXX_mullo_epi32(mid_tile_y, _mmXXX_set1_epi32(ctx->cov_tile_w)));

    while (tri_mask) {
        const SWint tri_ndx = _swGetFirstBit(tri_mask);
        tri_mask &= tri_mask - 1;

        const SWint tri_bb_width = bb_tile_size_x.i32[tri_ndx];
        const SWint tri_bb_height = bb_tile_size_y.i32[tri_ndx];

        const SWint tile_row_ndx = bb_bottom_ndx.i32[tri_ndx];
        const SWint tile_mid_row_ndx = bb_mid_ndx.i32[tri_ndx];
        const SWint tile_end_row_ndx = bb_top_ndx.i32[tri_ndx];

        const SWint tri_mid_vtx_right = (mid_vtx_right >> tri_ndx) & 1;
        const SWint tri_flat_bottom = (flat_bottom >> tri_ndx) & 1;

        if (tri_bb_width > 3 && tri_bb_height > 3) {
            if (tri_mid_vtx_right) {
                NAME(_swRasterizeTriangle_tight_mid_right)
                (ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx, tri_bb_width,
                 tri_ndx, slope_tile_delta[0].i32, event_start[0].i32, slope_fp[0].i32,
                 tri_flat_bottom);
            } else {
                NAME(_swRasterizeTriangle_tight_mid_left)
                (ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx, tri_bb_width,
                 tri_ndx, slope_tile_delta[0].i32, event_start[0].i32, slope_fp[0].i32,
                 tri_flat_bottom);
            }
        } else {
            if (tri_mid_vtx_right) {
                NAME(_swRasterizeTriangle_mid_right)
                (ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx, tri_bb_width,
                 tri_ndx, slope_tile_delta[0].i32, event_start[0].i32, slope_fp[0].i32,
                 tri_flat_bottom);
            } else {
                NAME(_swRasterizeTriangle_mid_left)
                (ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx, tri_bb_width,
                 tri_ndx, slope_tile_delta[0].i32, event_start[0].i32, slope_fp[0].i32,
                 tri_flat_bottom);
            }
        }
    }
}

SWint _swClipPolygon(const __m128 in_vtx[], const SWint in_vtx_count, const __m128 plane,
                     __m128 out_vtx[]);

#define SW_MAX_CLIPPED (8 * SIMD_WIDTH)

void NAME(_swProcessTrianglesIndexed)(SWcull_ctx *ctx, const void *attribs,
                                      const SWuint *indices, const SWuint stride,
                                      const SWuint index_count, const SWfloat *xform) {
    union {
        __m128 vec;
        float f32[4];
    } clip_tris[SW_MAX_CLIPPED * 3];
    SWint clip_head = 0, clip_tail = 0;

    SWuint tris_count = index_count / 3;
    while (tris_count || clip_head != clip_tail) {
        union {
            __mXXX vec;
            SWfloat f32[SIMD_WIDTH];
        } vX[3], vY[3], vW[3]; // 1/w is used instead of Z

        SWuint tri_mask;

        if (clip_head == clip_tail) { // clip buffer is empty
            const SWuint lanes_count = sw_min(tris_count, SIMD_WIDTH);
            tri_mask = (1u << lanes_count) - 1;

            //
            // Gather new vertices
            //
            for (SWuint tri = 0; tri < lanes_count; tri++) {
                for (SWint i = 0; i < 3; i++) {
                    const SWuint ii = indices[3 * tri + i];
                    vX[2 - i].f32[tri] =
                        ((SWfloat *)((uintptr_t)attribs + ii * stride))[0];
                    vY[2 - i].f32[tri] =
                        ((SWfloat *)((uintptr_t)attribs + ii * stride))[1];
                    vW[2 - i].f32[tri] =
                        ((SWfloat *)((uintptr_t)attribs + ii * stride))[2];
                }
            }

            tris_count -= lanes_count;
            indices += lanes_count * 3;

            //
            // Transform vertices
            //
            for (SWint i = 0; i < 3; i++) {
                // tmp_x = vX[i] * xform[0] + vY[i] * xform[4] + vZ[i] * xform[8] +
                // xform[12]
                __mXXX tmp_x = _mmXXX_fmadd_ps(
                    vX[i].vec, _mmXXX_set1_ps(xform[0]),
                    _mmXXX_fmadd_ps(vY[i].vec, _mmXXX_set1_ps(xform[4]),
                                    _mmXXX_fmadd_ps(vW[i].vec, _mmXXX_set1_ps(xform[8]),
                                                    _mmXXX_set1_ps(xform[12]))));
                // tmp_y = vX[i] * xform[1] + vY[i] * xform[5] + vZ[i] * xform[9] +
                // xform[13]
                __mXXX tmp_y = _mmXXX_fmadd_ps(
                    vX[i].vec, _mmXXX_set1_ps(xform[1]),
                    _mmXXX_fmadd_ps(vY[i].vec, _mmXXX_set1_ps(xform[5]),
                                    _mmXXX_fmadd_ps(vW[i].vec, _mmXXX_set1_ps(xform[9]),
                                                    _mmXXX_set1_ps(xform[13]))));
                // tmpW = vX[i] * xform[3] + vY[i] * xform[7] + vZ[i] * xform[11] +
                // xform[15]
                __mXXX tmp_w = _mmXXX_fmadd_ps(
                    vX[i].vec, _mmXXX_set1_ps(xform[3]),
                    _mmXXX_fmadd_ps(vY[i].vec, _mmXXX_set1_ps(xform[7]),
                                    _mmXXX_fmadd_ps(vW[i].vec, _mmXXX_set1_ps(xform[11]),
                                                    _mmXXX_set1_ps(xform[15]))));

                vX[i].vec = tmp_x;
                vY[i].vec = tmp_y;
                vW[i].vec = tmp_w;
            }

            //
            // Clip new triangles
            //

            // mask with triangles that require clipping
            SWuint intersect_mask[_PlanesCount];

            for (SWint plane = 0; plane < _PlanesCount; plane++) {
                __mXXX plane_dp[3]; // plane dot products
                for (SWint i = 0; i < 3; i++) {
                    switch (plane) {
                    case Left:
                        plane_dp[i] = _mmXXX_add_ps(vW[i].vec, vX[i].vec);
                        break;
                    case Right:
                        plane_dp[i] = _mmXXX_sub_ps(vW[i].vec, vX[i].vec);
                        break;
                    case Top:
                        plane_dp[i] = _mmXXX_sub_ps(vW[i].vec, vY[i].vec);
                        break;
                    case Bottom:
                        plane_dp[i] = _mmXXX_add_ps(vW[i].vec, vY[i].vec);
                        break;
                    case Near:
                        plane_dp[i] = _mmXXX_sub_ps(vW[i].vec,
                                                    _mmXXX_set1_ps(0.0f) /* near_dist */);
                        break;
                    }
                }

                // all dot products are negative
                const __mXXX outside =
                    _mmXXX_and_ps(plane_dp[0], _mmXXX_and_ps(plane_dp[1], plane_dp[2]));
                // all dot products are positive
                const __mXXX inside = _mmXXX_andnot_ps(
                    plane_dp[0],
                    _mmXXX_andnot_ps(plane_dp[1], _mmXXX_not_ps(plane_dp[2])));

                const SWuint outside_mask = (SWuint)_mmXXX_movemask_ps(outside);
                const SWuint inside_mask = (SWuint)_mmXXX_movemask_ps(inside);

                intersect_mask[plane] = (~outside_mask) & (~inside_mask);
                tri_mask &= (~outside_mask);
            }

            SWuint clip_mask = tri_mask;
            const SWuint clip_intersect_mask =
                (intersect_mask[0] | intersect_mask[1] | intersect_mask[2] |
                 intersect_mask[3] | intersect_mask[4]) &
                clip_mask;
            if (clip_intersect_mask) {
                __m128 temp_vtx_buf[2][8];

                while (clip_mask) {
                    const SWuint tri = (SWuint)_swGetFirstBit((long)clip_mask);
                    const SWuint tri_bit = (1u << tri);
                    clip_mask &= clip_mask - 1;

                    SWint curr_buf_ndx = 0;
                    SWint clipped_vtx_count = 3;
                    // unpack 3 initial vertices
                    for (SWint i = 0; i < 3; i++) {
                        temp_vtx_buf[0][i] = _mm128_setr_ps(
                            vX[i].f32[tri], vY[i].f32[tri], vW[i].f32[tri], 1.0f);
                    }

                    for (SWint i = 0; i < _PlanesCount; i++) {
                        if (intersect_mask[i] & tri_bit) {
                            const SWint next_buf_ndx = (curr_buf_ndx + 1) % 2;
                            clipped_vtx_count = _swClipPolygon(
                                temp_vtx_buf[curr_buf_ndx], clipped_vtx_count,
                                ((__m128 *)ctx->clip_planes)[i],
                                temp_vtx_buf[next_buf_ndx]);
                            curr_buf_ndx = next_buf_ndx;
                        }
                    }

                    if (clipped_vtx_count >= 3) {
                        clip_tris[clip_head * 3 + 0].vec = temp_vtx_buf[curr_buf_ndx][0];
                        clip_tris[clip_head * 3 + 1].vec = temp_vtx_buf[curr_buf_ndx][1];
                        clip_tris[clip_head * 3 + 2].vec = temp_vtx_buf[curr_buf_ndx][2];
                        clip_head = (clip_head + 1) % SW_MAX_CLIPPED;
                        for (SWint i = 2; i < clipped_vtx_count - 1; i++) {
                            clip_tris[clip_head * 3 + 0].vec =
                                temp_vtx_buf[curr_buf_ndx][0];
                            clip_tris[clip_head * 3 + 1].vec =
                                temp_vtx_buf[curr_buf_ndx][i];
                            clip_tris[clip_head * 3 + 2].vec =
                                temp_vtx_buf[curr_buf_ndx][i + 1];
                            clip_head = (clip_head + 1) % SW_MAX_CLIPPED;
                        }
                    }
                }

                // all vertices were copied to clipped buffer
                tri_mask = 0;
            }
        }

        if (clip_head != clip_tail) {
            //
            // Get vertices from clipped buffer
            //
            SWint clipped_count = clip_head > clip_tail
                                      ? (clip_head - clip_tail)
                                      : (SW_MAX_CLIPPED + clip_head - clip_tail);
            clipped_count = sw_min(clipped_count, SIMD_WIDTH);

            for (SWint clip_tri = 0; clip_tri < clipped_count; clip_tri++) {
                const SWint tri_ndx = clip_tail * 3;
                for (SWint i = 0; i < 3; i++) {
                    vX[i].f32[clip_tri] = clip_tris[tri_ndx + i].f32[0];
                    vY[i].f32[clip_tri] = clip_tris[tri_ndx + i].f32[1];
                    vW[i].f32[clip_tri] = clip_tris[tri_ndx + i].f32[2];
                }
                clip_tail = (clip_tail + 1) % SW_MAX_CLIPPED;
            }

            tri_mask = (1u << clipped_count) - 1;
        }

        if (!tri_mask) {
            continue;
        }

        //
        // Project vertices
        //
        for (SWint i = 0; i < 3; i++) {
            const __mXXX rcpW = _mmXXX_div_ps(_mmXXX_set1_ps(1.0f), vW[i].vec);
            vX[i].vec = _mmXXX_ceil_ps(
                _mmXXX_fmadd_ps(_mmXXX_mul_ps(vX[i].vec, _mmXXX_set1_ps(ctx->half_w)),
                                rcpW, _mmXXX_set1_ps(ctx->half_w)));
            vY[i].vec = _mmXXX_floor_ps(
                _mmXXX_fmadd_ps(_mmXXX_mul_ps(vY[i].vec, _mmXXX_set1_ps(-ctx->half_h)),
                                rcpW, _mmXXX_set1_ps(ctx->half_h)));
            vW[i].vec = rcpW;
        }

        //
        // Backface test
        //
        const __mXXX tri_area1 = _mmXXX_mul_ps(_mmXXX_sub_ps(vX[1].vec, vX[0].vec),
                                               _mmXXX_sub_ps(vY[2].vec, vY[0].vec));
        const __mXXX tri_area2 = _mmXXX_mul_ps(_mmXXX_sub_ps(vX[0].vec, vX[2].vec),
                                               _mmXXX_sub_ps(vY[0].vec, vY[1].vec));
        const __mXXX tri_area = _mmXXX_sub_ps(tri_area1, tri_area2);
        tri_mask &= _mmXXX_movemask_ps(_mmXXX_cmpgt_ps(tri_area, _mmXXX_set1_ps(0.0f)));

        if (!tri_mask) {
            continue;
        }

        NAME(_swProcessTriangleBatch)(ctx, &vX[0].vec, &vY[0].vec, &vW[0].vec, tri_mask);
    }
}