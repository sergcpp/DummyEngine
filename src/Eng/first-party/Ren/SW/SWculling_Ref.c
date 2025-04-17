#include "SWculling.h"

#include <float.h>

#include "SWrasterize.h"

#define SW_CULL_TILE_HEIGHT_SHIFT 0
#define SW_CULL_TILE_SIZE_Y (1 << SW_CULL_TILE_HEIGHT_SHIFT)

#define SW_CULL_SUBTILE_Y 1

#define FP_BITS 16

typedef struct SWztile {
    float zmin[2][4];
    uint32_t mask;
} SWztile;

#pragma warning(push)
#pragma warning(disable : 6385) // Reading invalid data

void _swComputeDepthPlane(const SWfloat v0[3], const SWfloat v1[3], const SWfloat v2[3],
                          SWfloat *z_px_dx, SWfloat *z_px_dy) {
    // depth plane z(x,y) = z0 + dx * x + dy * y
    const SWfloat x10 = v1[0] - v0[0];
    const SWfloat x20 = v2[0] - v0[0];
    const SWfloat y10 = v1[1] - v0[1];
    const SWfloat y20 = v2[1] - v0[1];
    const SWfloat z10 = v1[2] - v0[2];
    const SWfloat z20 = v2[2] - v0[2];
    const SWfloat d = ((SWfloat)1) / (x10 * y20 - y10 * x20);

    (*z_px_dx) = (z10 * y20 - y10 * z20) * d;
    (*z_px_dy) = (x10 * z20 - z10 * x20) * d;
}

void _swUpdateTileQuick(SWztile *tile, const uint32_t coverage_mask,
                        const SWfloat z_subtile_min[4]) {
    uint32_t mask = tile->mask;
    SWfloat *zmin0 = tile->zmin[0];
    SWfloat *zmin1 = tile->zmin[1];

    uint32_t rast_mask = coverage_mask;
    uint32_t dead_lane = 0;
    for (SWint j = 0; j < 4; j++) {
        if ((rast_mask & (0xff << (j * 8))) == 0 || z_subtile_min[j] < zmin0[j]) {
            dead_lane |= (0xff << j * 8);
        }
    }

    // don't update subtiles that fail depth test
    rast_mask = ~dead_lane & rast_mask;

    // discard layer 1 if triangle is significantly closer to observer
    uint32_t covered_lane = 0;
    for (SWint j = 0; j < 4; j++) {
        if (((rast_mask >> j * 8) & 0xff) == 0xff) {
            covered_lane |= (0xff << j * 8);
        }
    }
    uint32_t diff_neg = 0;
    for (SWint j = 0; j < 4; j++) {
        SWfloat diff = 2 * zmin1[j] - (z_subtile_min[j] + zmin0[j]);
        if (diff < 0) {
            diff_neg |= (0xff << j * 8);
        }
    }
    uint32_t discard_layer_mask = ~dead_lane & (diff_neg | covered_lane);

    // update mask
    mask = (~discard_layer_mask & mask) | rast_mask;

    // compute new value of zmin1, there are 4 cases:
    // zmin1 = min(zmin1, z_subtile_min) -> layer is updated
    // zmin1 = z_subtile_min             -> layer is discarded
    // zmin1 = FLT_MAX                   -> layer is fully covered
    // zmin1 unchanged                   -> layer is not updated

    for (SWint j = 0; j < 4; j++) {
        SWfloat op_a = (dead_lane & (0xff << j * 8)) ? zmin1[j] : z_subtile_min[j];
        SWfloat op_b =
            (discard_layer_mask & (0xff << j * 8)) ? z_subtile_min[j] : zmin1[j];
        SWfloat z1min = sw_min(op_a, op_b);

        SWint mask_full = ((mask >> (j * 8)) & 0xff) == 0xff;

        zmin1[j] = mask_full ? FLT_MAX : z1min;
        zmin0[j] = mask_full ? z1min : zmin0[j];

        tile->mask &= ~(0xff << (j * 8));
        if (!mask_full) {
            tile->mask |= (mask & 0xff << (j * 8));
        }
    }
}

void _swUpdateTileAccurate(SWztile *tile, const uint32_t coverage_mask,
                           const SWfloat z_subtile_min[4]) {
    uint32_t mask = tile->mask;
    SWfloat *zmin0 = tile->zmin[0];
    SWfloat *zmin1 = tile->zmin[1];

    uint32_t rast_mask = coverage_mask;

    // perform depth tests with layer 0 and 1
    SWfloat sdist0[4], sdist1[4];
    uint32_t sign0 = 0, sign1 = 0;
    for (SWint j = 0; j < 4; j++) {
        sdist0[j] = zmin0[j] - z_subtile_min[j];
        sdist1[j] = zmin1[j] - z_subtile_min[j];

        if (sdist0[j] < 0) {
            sign0 |= (0xff << j * 8);
        }

        if (sdist1[j] < 0) {
            sign1 |= (0xff << j * 8);
        }
    }

    uint32_t tri_mask = rast_mask & ((~mask & sign0) | (mask & sign1));

    if (!tri_mask) {
        // early out
        return;
    }

    uint32_t t0 = 0;
    SWfloat z_tri[4];
    for (SWint j = 0; j < 4; j++) {
        if ((tri_mask & (0xff << (j * 8))) == 0) {
            t0 |= (0xff << j * 8);
            z_tri[j] = zmin0[j];
        } else {
            z_tri[j] = z_subtile_min[j];
        }
    }
    (void)t0;

    // test if incoming triangle overwrites layer 0 or 1 completely
    uint32_t layer0_mask = (~tri_mask & ~mask);
    uint32_t layer1_mask = (~tri_mask & mask);
    uint32_t lm0 = 0, lm1 = 0;
    SWfloat z0[4], z1[4];
    for (SWint j = 0; j < 4; j++) {
        if ((layer0_mask & (0xff << j * 8)) == 0) {
            lm0 |= (0xff << j * 8);
            z0[j] = z_tri[j];
        } else {
            z0[j] = zmin0[j];
        }

        if ((layer1_mask & (0xff << j * 8)) == 0) {
            lm1 |= (0xff << j * 8);
            z1[j] = z_tri[j];
        } else {
            z1[j] = zmin1[j];
        }
    }

    // distances used for merging heuristic
    SWfloat d0[4], d1[4], d2[4];
    for (SWint j = 0; j < 4; j++) {
        d0[j] = sw_abs(sdist0[j]);
        d1[j] = sw_abs(sdist1[j]);
        d2[j] = sw_abs(z0[j] - z1[j]);
    }

    // find min dist
    SWint c01[4], c02[4], c12[4];
    SWint d0min[4], d1min[4];
    for (SWint j = 0; j < 4; j++) {
        c01[j] = (SWint)(d0[j] - d1[j]);
        c02[j] = (SWint)(d0[j] - d2[j]);
        c12[j] = (SWint)(d1[j] - d2[j]);

        if ((c01[j] < 0 && c02[j] < 0) || (lm0 & (0xff << j * 8)) ||
            ((tri_mask & (0xff << j * 8)) == 0)) {
            d0min[j] = -1;
        } else {
            d0min[j] = 1;
        }

        if ((d0min[j] >= 0) && (c12[j] < 0 || (lm1 & (0xff << j * 8)))) {
            d1min[j] = -1;
        } else {
            d1min[j] = 1;
        }
    }

    // update mask based on which layer the triangle overwrites or was merged into
    uint32_t inner = 0;
    for (SWint j = 0; j < 4; j++) {
        if (d0min[j] < 0) {
            inner |= (layer1_mask & (0xff << j * 8));
        } else {
            inner |= (tri_mask & (0xff << j * 8));
        }
    }
    tile->mask = 0;
    for (SWint j = 0; j < 4; j++) {
        if (d1min[j] < 0) {
            tile->mask |= (layer0_mask & (0xff << j * 8));
        } else {
            tile->mask |= (inner & (0xff << j * 8));
        }
    }

    for (SWint j = 0; j < 4; j++) {
        SWfloat e0 = (d1min[j] < 0) ? z1[j] : z0[j];
        SWfloat e1 = (d1min[j] < 0 || d0min[j] < 0) ? z_tri[j] : z1[j];
        zmin0[j] = sw_min(e0, e1);

        SWfloat z1t = (d0min[j] < 0) ? z1[j] : z_tri[j];
        zmin1[j] = (d1min[j] < 0) ? z0[j] : z1t;
    }
}

SWint _swProcessScanline_Ref(SWztile ztiles[], const SWint left_offset,
                             const SWint right_offset, const SWint left_event,
                             const SWint left_count, const SWint right_event,
                             const SWint right_count, const SWint events[3],
                             SWint tile_ndx, const SWfloat zmin_tri,
                             const SWfloat zmax_tri, const SWfloat _z0[4],
                             const SWfloat z_dx, SWint is_occluder) {
    SWint event_offset = (left_offset << SW_CULL_TILE_WIDTH_SHIFT);

    SWint left[2], right[2];
    for (SWint i = 0; i < left_count; i++) {
        left[i] = sw_max((events[left_event - i] >> FP_BITS) - event_offset, 0);
    }
    for (SWint i = 0; i < right_count; i++) {
        right[i] = sw_max((events[right_event + i] >> FP_BITS) - event_offset, 0);
    }

    SWfloat z0[4];
    for (SWint j = 0; j < 4; j++) {
        z0[j] = _z0[j] + z_dx * left_offset;
    }

    SWint tile_ndx_end = tile_ndx + right_offset;
    tile_ndx += left_offset;

    while (1) {
#ifdef SW_CULL_QUICK_MASK
        const SWfloat *zmin0_buf = ztiles[tile_ndx].zmin[0];
#else
        uint32_t mask = ztiles[tile_ndx].mask;
        SWfloat zmin0_buf[4];
        for (SWint j = 0; j < 4; j++) {
            SWfloat zmin0 = ztiles[tile_ndx].zmin[0][j];
            if (((mask >> (j * 8)) & 0xff) == 0xff) {
                zmin0 = ztiles[tile_ndx].zmin[1][j];
            }
            SWfloat zmin1 = ztiles[tile_ndx].zmin[1][j];
            if (((mask >> (j * 8)) & 0xff) == 0) {
                zmin1 = ztiles[tile_ndx].zmin[0][j];
            }

            zmin0_buf[j] = sw_min(zmin0, zmin1);
        }
#endif

        SWint all_dist_neg = 1;
        for (SWint j = 0; j < 4; j++) {
            SWfloat dist = zmax_tri - zmin0_buf[j];
            all_dist_neg &= (dist < 0);
        }

        if (!all_dist_neg) {
            uint32_t coverage_mask = (0xffffffffULL << sw_min(left[0], 32));
            for (SWint i = 1; i < left_count; i++) {
                coverage_mask &= (0xffffffffULL << sw_min(left[i], 32));
            }
            for (SWint i = 0; i < right_count; i++) {
                coverage_mask &= ~(0xffffffffULL << sw_min(right[i], 32));
            }

            if (is_occluder) {
                // compute interpolated min for each subtile and update tile
                SWfloat z_subtile_min[4];
                for (SWint j = 0; j < 4; j++) {
                    z_subtile_min[j] = sw_max(z0[j], zmin_tri);
                }

#ifdef SW_CULL_QUICK_MASK
                _swUpdateTileQuick(&ztiles[tile_ndx], coverage_mask, z_subtile_min);
#else
                _swUpdateTileAccurate(&ztiles[tile_ndx], coverage_mask, z_subtile_min);
#endif
            } else { // occludee case
                // conservative visibility test
                SWfloat subtile_zmax[4];
                SWuint zpass = 0;
                for (SWint j = 0; j < 4; j++) {
                    subtile_zmax[j] = sw_min(z0[j], zmax_tri);
                    if (subtile_zmax[j] >= zmin0_buf[j]) {
                        zpass |= (0xff << (j * 8));
                    }
                }

                uint32_t rast_mask = coverage_mask;
                uint8_t dead_lane[4];
                for (SWint j = 0; j < 4; j++) {
                    dead_lane[j] = (rast_mask & (0xff << (j * 8))) == 0;
                    if (dead_lane[j]) {
                        zpass &= ~(0xff << (j * 8));
                    }
                }

                if (zpass) {
                    return 1;
                }
            }
        }

        if (++tile_ndx >= tile_ndx_end) {
            break;
        }

        for (SWint j = 0; j < 4; j++) {
            z0[j] += z_dx;
        }
        for (SWint i = 0; i < left_count; i++) {
            left[i] = sw_max(left[i] - SW_CULL_TILE_SIZE_X, 0);
        }
        for (SWint i = 0; i < right_count; i++) {
            right[i] = sw_max(right[i] - SW_CULL_TILE_SIZE_X, 0);
        }
    }

    return is_occluder;
}

SWint _swRasterizeTriangle_Ref(SWcull_ctx *ctx, SWint tile_row_ndx,
                               const SWint tile_mid_row_ndx, const SWint tile_end_row_ndx,
                               const SWint bb_width, SWint slope_tile_delta[3],
                               SWint event_start[3], const SWfloat zmin,
                               const SWfloat zmax, SWfloat z0[4], const SWfloat z_dx,
                               const SWfloat z_dy, const SWint mid_vtx_right,
                               const SWint use_tight_traversal, const SWint flat_bottom,
                               const SWint is_occluder) {
    SWztile *ztiles = (SWztile *)ctx->ztiles;
    const SWint tile_count = ctx->tile_w * ctx->tile_h;

#define LEFT_EDGE_BIAS 0
#define RIGHT_EDGE_BIAS 0
#define UPDATE_TILE_EVENTS_Y(i) tri_event[i] += tri_slope_tile_delta[i];

    SWint tri_slope_tile_delta[3];
    tri_slope_tile_delta[0] = slope_tile_delta[0];
    tri_slope_tile_delta[1] = slope_tile_delta[1];
    tri_slope_tile_delta[2] = slope_tile_delta[2];

    SWint tri_event[3];
    tri_event[0] = event_start[0];
    tri_event[1] = event_start[1];
    tri_event[2] = event_start[2];

    SWint start_delta = 0, end_delta = 0, top_delta = 0, start_event = 0, end_event = 0, top_event = 0;
    if (use_tight_traversal) {
        start_delta = slope_tile_delta[2] + LEFT_EDGE_BIAS;
        end_delta = slope_tile_delta[0] + RIGHT_EDGE_BIAS;
        top_delta =
            slope_tile_delta[1] + (mid_vtx_right ? RIGHT_EDGE_BIAS : LEFT_EDGE_BIAS);

        start_event = event_start[2] + sw_min(0, start_delta);
        end_event =
            event_start[0] + sw_max(0, end_delta) + (SW_CULL_TILE_SIZE_X << FP_BITS);
        if (mid_vtx_right) {
            top_event =
                event_start[1] + sw_max(0, top_delta) + (SW_CULL_TILE_SIZE_X << FP_BITS);
        } else {
            top_event = event_start[1] + sw_min(0, top_delta);
        }
    }

    if (!flat_bottom) {
        SWint tile_stop_ndx = sw_min(tile_end_row_ndx, tile_mid_row_ndx);
        // bottom half of triangle
        while (tile_row_ndx < tile_stop_ndx) {
            SWint start = 0, end = bb_width;
            if (use_tight_traversal) {
                start = sw_max(
                    0, sw_min(bb_width - 1,
                              start_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                end =
                    sw_min(bb_width, (end_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                start_event += start_delta;
                end_event += end_delta;
            }

            assert(tile_row_ndx + start <= tile_count &&
                   tile_row_ndx + end <= tile_count);
            const SWint res = _swProcessScanline_Ref(
                ztiles, start, end, 2 /* left_event */, 1, 0 /* right_event*/, 1,
                tri_event, tile_row_ndx, zmin, zmax, z0, z_dx, is_occluder);
            if (res && !is_occluder) {
                return 1;
            }

            tile_row_ndx += ctx->tile_w;
            for (SWint j = 0; j < 4; j++) {
                z0[j] += z_dy;
            }
            UPDATE_TILE_EVENTS_Y(0)
            UPDATE_TILE_EVENTS_Y(2)
        }

        // middle part (toched by all three edges)
        if (tile_row_ndx < tile_end_row_ndx) {
            SWint start = 0, end = bb_width;
            if (use_tight_traversal) {
                start = sw_max(
                    0, sw_min(bb_width - 1,
                              start_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                end =
                    sw_min(bb_width, (end_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));

                end_event = mid_vtx_right ? top_event : end_event;
                end_delta = mid_vtx_right ? top_delta : end_delta;
                start_event = mid_vtx_right ? start_event : top_event;
                start_delta = mid_vtx_right ? start_delta : top_delta;
                start_event += start_delta;
                end_event += end_delta;
            }

            assert(tile_row_ndx + start <= tile_count &&
                   tile_row_ndx + end <= tile_count);

            SWint res;
            if (mid_vtx_right) {
                res = _swProcessScanline_Ref(
                    ztiles, start, end, 2 /* left_event */, 1, 0 /* right_event*/, 2,
                    tri_event, tile_row_ndx, zmin, zmax, z0, z_dx, is_occluder);
            } else {
                res = _swProcessScanline_Ref(
                    ztiles, start, end, 2 /* left_event */, 2, 0 /* right_event*/, 1,
                    tri_event, tile_row_ndx, zmin, zmax, z0, z_dx, is_occluder);
            }
            if (res && !is_occluder) {
                return 1;
            }

            tile_row_ndx += ctx->tile_w;
        }

        // top half of triangle
        if (tile_row_ndx < tile_end_row_ndx) {
            for (SWint j = 0; j < 4; j++) {
                z0[j] += z_dy;
            }
            SWint i0 = mid_vtx_right + 0;
            SWint i1 = mid_vtx_right + 1;
            UPDATE_TILE_EVENTS_Y(i0);
            UPDATE_TILE_EVENTS_Y(i1);

            while (1) {
                SWint start = 0, end = bb_width;
                if (use_tight_traversal) {
                    start = sw_max(
                        0, sw_min(bb_width - 1,
                                  start_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                    end = sw_min(bb_width,
                                 (end_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                    start_event += start_delta;
                    end_event += end_delta;
                }

                assert(tile_row_ndx + start <= tile_count &&
                       tile_row_ndx + end <= tile_count);
                const SWint res = _swProcessScanline_Ref(
                    ztiles, start, end, mid_vtx_right + 1 /* left_event */, 1,
                    mid_vtx_right + 0 /* right_event*/, 1, tri_event, tile_row_ndx, zmin,
                    zmax, z0, z_dx, is_occluder);
                if (res && !is_occluder) {
                    return 1;
                }

                tile_row_ndx += ctx->tile_w;
                if (tile_row_ndx >= tile_end_row_ndx) {
                    break;
                }

                for (SWint j = 0; j < 4; j++) {
                    z0[j] += z_dy;
                }
                UPDATE_TILE_EVENTS_Y(i0);
                UPDATE_TILE_EVENTS_Y(i1);
            }
        }
    } else {
        if (use_tight_traversal) {
            end_event = mid_vtx_right ? top_event : end_event;
            end_delta = mid_vtx_right ? top_delta : end_delta;
            start_event = mid_vtx_right ? start_event : top_event;
            start_delta = mid_vtx_right ? start_delta : top_delta;
        }

        // top half of triangle
        if (tile_row_ndx < tile_end_row_ndx) {
            SWint i0 = mid_vtx_right + 0;
            SWint i1 = mid_vtx_right + 1;

            while (1) {
                SWint start = 0, end = bb_width;
                if (use_tight_traversal) {
                    start = sw_max(
                        0, sw_min(bb_width - 1,
                                  start_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                    end = sw_min(bb_width,
                                 (end_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                    start_event += start_delta;
                    end_event += end_delta;
                }

                assert(tile_row_ndx + start <= tile_count &&
                       tile_row_ndx + end <= tile_count);
                const SWint res = _swProcessScanline_Ref(
                    ztiles, start, end, mid_vtx_right + 1 /* left_event */, 1,
                    mid_vtx_right + 0 /* right_event*/, 1, tri_event, tile_row_ndx, zmin,
                    zmax, z0, z_dx, is_occluder);
                if (res && !is_occluder) {
                    return 1;
                }

                tile_row_ndx += ctx->tile_w;
                if (tile_row_ndx >= tile_end_row_ndx) {
                    break;
                }

                for (SWint j = 0; j < 4; j++) {
                    z0[j] += z_dy;
                }
                UPDATE_TILE_EVENTS_Y(i0);
                UPDATE_TILE_EVENTS_Y(i1);
            }
        }
    }

    (void)tile_count;

    return is_occluder;

#undef LEFT_EDGE_BIAS
#undef RIGHT_EDGE_BIAS
#undef UPDATE_TILE_EVENTS_Y
}

SWint _swProcessTriangle_Ref(SWcull_ctx *ctx, SWfloat v0[3], SWfloat v1[3], SWfloat v2[3],
                             SWint is_occluder) {
    //const SWint tile_count = ctx->tile_w * ctx->tile_h;

    SWint bb_min[2], bb_max[2];

    // find triangle bounds
    bb_min[0] = (SWint)sw_min(v0[0], sw_min(v1[0], v2[0]));
    bb_min[1] = (SWint)sw_min(v0[1], sw_min(v1[1], v2[1]));
    bb_max[0] = (SWint)sw_max(v0[0], sw_max(v1[0], v2[0]));
    bb_max[1] = (SWint)sw_max(v0[1], sw_max(v1[1], v2[1]));

    // snap to tiles (min % TILE_SIZE_, (max + TILE_SIZE_ - 1) % TILE_SIZE_)
    bb_min[0] &= ~(SW_CULL_TILE_SIZE_X - 1);
    bb_min[1] &= ~(SW_CULL_TILE_SIZE_Y - 1);

    bb_max[0] += SW_CULL_TILE_SIZE_X - 1;
    bb_max[1] += SW_CULL_TILE_SIZE_Y - 1;

    bb_max[0] &= ~(SW_CULL_TILE_SIZE_X - 1);
    bb_max[1] &= ~(SW_CULL_TILE_SIZE_Y - 1);

    // clamp to frame bounds
    bb_min[0] = sw_max(bb_min[0], 0);
    bb_min[1] = sw_max(bb_min[1], 0);
    bb_max[0] = sw_min(bb_max[0], ctx->tile_w * SW_CULL_TILE_SIZE_X);
    bb_max[1] = sw_min(bb_max[1], ctx->tile_h * SW_CULL_TILE_SIZE_Y);

    if (bb_min[0] == bb_max[0] || bb_min[1] == bb_max[1]) {
        return 0;
    }

    SWfloat z_px_dx, z_px_dy;
    _swComputeDepthPlane(v0, v1, v2, &z_px_dx, &z_px_dy);

    // compute z value at bounding box corner
    const SWfloat bb_min_v0[2] = {((SWfloat)bb_min[0]) - v0[0],
                                  ((SWfloat)bb_min[1]) - v0[1]};
    SWfloat z_plane_offset = z_px_dx * bb_min_v0[0] + z_px_dy * bb_min_v0[1] + v0[2];
    const SWfloat z_tile_dx = z_px_dx * ((SWfloat)SW_CULL_TILE_SIZE_X);
    const SWfloat z_tile_dy = z_px_dy * ((SWfloat)SW_CULL_TILE_SIZE_Y);

    if (is_occluder) {
        z_plane_offset += sw_min(z_px_dx * SW_CULL_SUBTILE_X, 0);
        z_plane_offset += sw_min(z_px_dy * SW_CULL_SUBTILE_Y, 0);
    } else {
        z_plane_offset += sw_max(z_px_dx * SW_CULL_SUBTILE_X, 0);
        z_plane_offset += sw_max(z_px_dy * SW_CULL_SUBTILE_Y, 0);
    }

    const SWfloat zmin = sw_min(v0[2], sw_min(v1[2], v2[2]));
    const SWfloat zmax = sw_max(v0[2], sw_max(v1[2], v2[2]));

    // Rotate vertices in winding order until p0 ends up at the bottom
    for (SWint i = 0; i < 2; i++) {
        SWfloat ey1 = v1[1] - v0[1];
        SWfloat ey2 = v2[1] - v0[1];

        if (ey1 < 0 || ey2 <= 0) {
            sw_rotate_leftf(v0[0], v1[0], v2[0]);
            sw_rotate_leftf(v0[1], v1[1], v2[1]);
        }
    }

    SWfloat edges[3][2];
    edges[0][0] = v1[0] - v0[0];
    edges[0][1] = v1[1] - v0[1];

    edges[1][0] = v2[0] - v1[0];
    edges[1][1] = v2[1] - v1[1];

    edges[2][0] = v2[0] - v0[0];
    edges[2][1] = v2[1] - v0[1];

    SWint mid_vtx_right = edges[1][1] >= 0 ? -1 : 0;

    SWfloat mid_pixel[2];
    if (edges[1][1] < 0) {
        mid_pixel[0] = v2[0];
        mid_pixel[1] = v2[1];
    } else {
        mid_pixel[0] = v1[0];
        mid_pixel[1] = v1[1];
    }

    SWint bb_tile_min[2] = {bb_min[0] / SW_CULL_TILE_SIZE_X,
                            bb_min[1] / SW_CULL_TILE_SIZE_Y};
    SWint bb_tile_max[2] = {bb_max[0] / SW_CULL_TILE_SIZE_X,
                            bb_max[1] / SW_CULL_TILE_SIZE_Y};

    SWint mid_tile_y = sw_max(((SWint)mid_pixel[1]), 0) / SW_CULL_TILE_SIZE_Y;
    SWint bb_mid_tile_y = sw_max(bb_tile_min[1], sw_min(bb_tile_max[1], mid_tile_y));

    SWfloat slope[3];
    slope[0] = edges[0][0] / edges[0][1];
    slope[1] = edges[1][0] / edges[1][1];
    slope[2] = edges[2][0] / edges[2][1];

    SWfloat horizontal_slope_delta = ((SWfloat)ctx->w) + 2.0f * (1.0f + 1.0f);
    if (edges[0][1] == (SWfloat)0) {
        slope[0] = horizontal_slope_delta;
    }
    if (edges[1][1] == (SWfloat)0) {
        slope[1] = -horizontal_slope_delta;
    }

    SWint slopei_fp[3];
    slopei_fp[0] = (SWint)(slope[0] * (1 << FP_BITS));
    slopei_fp[1] = (SWint)(slope[1] * (1 << FP_BITS));
    slopei_fp[2] = (SWint)(slope[2] * (1 << FP_BITS));

    // avoid cracks
    slopei_fp[0] += 1;
    slopei_fp[1] += edges[1][1] < 0.0f ? 0 : 1;

    SWint slope_tile_delta[3];
    slope_tile_delta[0] = slopei_fp[0] * SW_CULL_TILE_SIZE_Y;
    slope_tile_delta[1] = slopei_fp[1] * SW_CULL_TILE_SIZE_Y;
    slope_tile_delta[2] = slopei_fp[2] * SW_CULL_TILE_SIZE_Y;

    SWint diffi[2][2];
    diffi[0][0] = (((SWint)v0[0]) - bb_min[0]) << FP_BITS;
    diffi[1][0] = (((SWint)mid_pixel[0]) - bb_min[0]) << FP_BITS;

    diffi[0][1] = ((SWint)v0[1]) - bb_min[1];
    diffi[1][1] = ((SWint)mid_pixel[1]) - bb_mid_tile_y * SW_CULL_TILE_SIZE_Y;

    SWint event_start[3];
    event_start[0] = diffi[0][0] - slopei_fp[0] * diffi[0][1];
    event_start[1] = diffi[1][0] - slopei_fp[1] * diffi[1][1];
    event_start[2] = diffi[0][0] - slopei_fp[2] * diffi[0][1];

    //
    // Split bounding box into bottom - middle - top region
    //

    SWint bbox_bottom_ndx = bb_tile_min[0] + bb_tile_min[1] * ctx->tile_w;
    SWint bbox_top_ndx = bb_tile_min[0] + bb_tile_max[1] * ctx->tile_w;
    SWint bbox_mid_ndx = bb_tile_min[0] + bb_mid_tile_y * ctx->tile_w;

    SWint res = 0;

    { // Rasterize triangle
        SWint bb_width = bb_tile_max[0] - bb_tile_min[0];
        SWint bb_height = bb_tile_max[1] - bb_tile_min[1];

        SWfloat tri_zmin = zmin;
        SWfloat tri_zmax = zmax;

        SWfloat z0[4] = {z_plane_offset + z_px_dy * 0 + z_px_dx * 0 * SW_CULL_SUBTILE_X,
                         z_plane_offset + z_px_dy * 0 + z_px_dx * 1 * SW_CULL_SUBTILE_X,
                         z_plane_offset + z_px_dy * 0 + z_px_dx * 2 * SW_CULL_SUBTILE_X,
                         z_plane_offset + z_px_dy * 0 + z_px_dx * 3 * SW_CULL_SUBTILE_X};

        SWint tile_row_ndx = bbox_bottom_ndx;
        SWint tile_mid_row_ndx = bbox_mid_ndx;
        SWint tile_end_row_ndx = bbox_top_ndx;

        mid_vtx_right = sw_abs(mid_vtx_right);

        // Skip empty areas for big triangles
        const SWint tight_traversal = bb_width > 3 && bb_height > 3;

        const SWint flat_bottom = (bb_min[1] == mid_pixel[1]);

        res |= _swRasterizeTriangle_Ref(
            ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx, bb_width,
            slope_tile_delta, event_start, tri_zmin, tri_zmax, z0, z_tile_dx, z_tile_dy,
            mid_vtx_right, tight_traversal, flat_bottom, is_occluder);

        if (res && !is_occluder) {
            return 1;
        }
    }

    return res;
}

#undef SIMD_WIDTH
#define SIMD_WIDTH 1
#define SW_MAX_CLIPPED (8 * SIMD_WIDTH)

SWint _swProcessTrianglesIndexed_Ref(SWcull_ctx *ctx, const void *attribs,
                                     const SWuint *indices, const SWuint stride,
                                     const SWuint index_count, const SWfloat *xform,
                                     SWint is_occluder) {
    union {
        __m128 vec;
        float f32[4];
    } clipped_tris[SW_MAX_CLIPPED * 3];
    SWint clip_head = 0, clip_tail = 0;

    SWint res = 0;

    SWuint tris_count = index_count / 3;
    while (tris_count || clip_head != clip_tail) {
        SWfloat vX[3], vY[3], vW[3]; // 1/w is used instead of Z

        SWuint tri_mask = 0;
        if (clip_head == clip_tail) { // clip buffer is empty
            //
            // Gather new vertices
            //
            for (SWint i = 0; i < 3; i++) {
                const SWuint ii = indices[i];
                vX[2 - i] = ((SWfloat *)((uintptr_t)attribs + ii * stride))[0];
                vY[2 - i] = ((SWfloat *)((uintptr_t)attribs + ii * stride))[1];
                vW[2 - i] = ((SWfloat *)((uintptr_t)attribs + ii * stride))[2];
            }

            tri_mask = 1;
            tris_count -= 1;
            indices += 3;

            //
            // Transform vertices
            //
            for (SWint i = 0; i < 3; i++) {
                const SWfloat tmp_x =
                    vX[i] * xform[0] + vY[i] * xform[4] + vW[i] * xform[8] + xform[12];
                const SWfloat tmp_y =
                    vX[i] * xform[1] + vY[i] * xform[5] + vW[i] * xform[9] + xform[13];
                const SWfloat tmp_w =
                    vX[i] * xform[3] + vY[i] * xform[7] + vW[i] * xform[11] + xform[15];

                vX[i] = tmp_x;
                vY[i] = tmp_y;
                vW[i] = tmp_w;
            }

            //
            // Clip new triangles
            //
            SWuint intersects[_PlanesCount];
            for (SWint plane = 0; plane < _PlanesCount; plane++) {
                SWfloat plane_dp[3]; // plane dot products
                for (SWint i = 0; i < 3; i++) {
                    switch (plane) {
                    case Left:
                        plane_dp[i] = vW[i] + vX[i];
                        break;
                    case Right:
                        plane_dp[i] = vW[i] - vX[i];
                        break;
                    case Top:
                        plane_dp[i] = vW[i] - vY[i];
                        break;
                    case Bottom:
                        plane_dp[i] = vW[i] + vY[i];
                        break;
                    case Near:
                        plane_dp[i] = vW[i] - ctx->near_clip;
                        break;
                    }
                }

                const SWuint fully_outside =
                    plane_dp[0] < 0.0f && plane_dp[1] < 0.0f && plane_dp[2] < 0.0f;
                const SWuint fully_inside =
                    plane_dp[0] >= 0.0f && plane_dp[1] >= 0.0f && plane_dp[2] >= 0.0f;

                intersects[plane] = !fully_outside && !fully_inside;
                tri_mask &= ~fully_outside;
            }

            const SWuint needs_clipping =
                (intersects[0] || intersects[1] || intersects[2] || intersects[3] ||
                 intersects[4]) &
                tri_mask;

            if (needs_clipping) {
                __m128 temp_vtx_buf[2][8];

                SWint curr_buf_ndx = 0;
                SWint clipped_vtx_count = 3;
                // unpack 3 initial vertices
                for (SWint i = 0; i < 3; i++) {
                    temp_vtx_buf[0][i] = _mm128_setr_ps(vX[i], vY[i], vW[i], 1.0f);
                }

                for (SWint i = 0; i < _PlanesCount; i++) {
                    if (intersects[i]) {
                        const SWint next_buf_ndx = (curr_buf_ndx + 1) % 2;
                        clipped_vtx_count = _swClipPolygon(
                            temp_vtx_buf[curr_buf_ndx], clipped_vtx_count,
                            ((__m128 *)ctx->clip_planes)[i], temp_vtx_buf[next_buf_ndx]);
                        curr_buf_ndx = next_buf_ndx;
                    }
                }

                if (clipped_vtx_count >= 3) {
                    clipped_tris[clip_head * 3 + 0].vec = temp_vtx_buf[curr_buf_ndx][0];
                    clipped_tris[clip_head * 3 + 1].vec = temp_vtx_buf[curr_buf_ndx][1];
                    clipped_tris[clip_head * 3 + 2].vec = temp_vtx_buf[curr_buf_ndx][2];
                    clip_head = (clip_head + 1) % SW_MAX_CLIPPED;
                    for (SWint i = 2; i < clipped_vtx_count - 1; i++) {
                        clipped_tris[clip_head * 3 + 0].vec =
                            temp_vtx_buf[curr_buf_ndx][0];
                        clipped_tris[clip_head * 3 + 1].vec =
                            temp_vtx_buf[curr_buf_ndx][i];
                        clipped_tris[clip_head * 3 + 2].vec =
                            temp_vtx_buf[curr_buf_ndx][i + 1];
                        clip_head = (clip_head + 1) % SW_MAX_CLIPPED;
                    }
                }

                tri_mask = 0;
            }
        }

        if (clip_head != clip_tail) {
            //
            // Get vertices from clipped buffer
            //
            const SWint tri_ndx = clip_tail * 3;
            for (SWint i = 0; i < 3; i++) {
                vX[i] = clipped_tris[tri_ndx + i].f32[0];
                vY[i] = clipped_tris[tri_ndx + i].f32[1];
                vW[i] = clipped_tris[tri_ndx + i].f32[2];
            }
            clip_tail = (clip_tail + 1) % SW_MAX_CLIPPED;
            tri_mask = 1;
        }

        if (!tri_mask) {
            continue;
        }

        //
        // Project vertices
        //

        for (SWint i = 0; i < 3; i++) {
            const SWfloat rcpW = 1 / vW[i];
            vX[i] = ceilf(vX[i] * ctx->half_w * rcpW + ctx->half_w);
            vY[i] = floorf(vY[i] * (-ctx->half_h) * rcpW + ctx->half_h);
            vW[i] = rcpW;
        }

        //
        // Backface test
        //
        const SWfloat tri_area1 = (vX[1] - vX[0]) * (vY[2] - vY[0]);
        const SWfloat tri_area2 = (vX[0] - vX[2]) * (vY[0] - vY[1]);
        const SWfloat tri_area = (tri_area1 - tri_area2);

        if (tri_area <= 0) {
            continue;
        }

        if (tri_mask) {
            SWfloat v0[3] = {vX[0], vY[0], vW[0]};
            SWfloat v1[3] = {vX[1], vY[1], vW[1]};
            SWfloat v2[3] = {vX[2], vY[2], vW[2]};

            res |= _swProcessTriangle_Ref(ctx, v0, v1, v2, is_occluder);
            if (res && !is_occluder) {
                return 1;
            }
        }
    }

    return res;
}

SWint _swCullCtxTestRect_Ref(const SWcull_ctx *ctx, const SWfloat p_min[2],
                             const SWfloat p_max[3], const SWfloat w_min) {
#define SIMD_TILE_PAD _mm128_setr_epi32(0, SW_CULL_TILE_SIZE_X - 1, 0, SW_CULL_TILE_SIZE_Y - 1)
#define SIMD_TILE_PAD_MASK                                                               \
    _mm128_setr_epi32(~(SW_CULL_TILE_SIZE_X - 1), ~(SW_CULL_TILE_SIZE_X - 1),            \
                      ~(SW_CULL_TILE_SIZE_Y - 1), ~(SW_CULL_TILE_SIZE_Y - 1))
#define SIMD_SUBTILE_PAD _mm128_setr_epi32(0, SW_CULL_SUBTILE_X, 0, SW_CULL_SUBTILE_Y)
#define SIMD_SUBTILE_PAD_MASK                                                            \
    _mm128_setr_epi32(~(SW_CULL_SUBTILE_X - 1), ~(SW_CULL_SUBTILE_X - 1),                \
                      ~(SW_CULL_SUBTILE_Y - 1), ~(SW_CULL_SUBTILE_Y - 1))

    const SWztile *ztiles = (SWztile *)ctx->ztiles;

    __m128i *size = (__m128i *)ctx->size_ivec4;
    __m128 *half_size = (__m128 *)ctx->half_size_vec4;

    if (p_min[0] > p_max[0] || p_min[1] > p_max[1]) {
        return 0;
    }

    __m128 px_bbox = _mm128_fmadd_ps(_mm128_setr_ps(p_min[0], p_max[0], p_min[1], p_max[1]),
                                     (*half_size), (*half_size));

    __m128i px_bboxi = _mm128_cvtps_epi32(px_bbox);
    px_bboxi = _mm128_max_epi32(px_bboxi, _mm128_setzero_si128());
    px_bboxi = _mm128_min_epi32(px_bboxi, (*size));

    union {
        __m128i vec;
        SWint i32[4];
    } tile_bboxi, subtile_bboxi;

    tile_bboxi.vec =
        _mm128_and_si128(_mm128_add_epi32(px_bboxi, SIMD_TILE_PAD), SIMD_TILE_PAD_MASK);
    SWint tile_min_x = tile_bboxi.i32[0] >> SW_CULL_TILE_WIDTH_SHIFT;
    SWint tile_max_x = tile_bboxi.i32[1] >> SW_CULL_TILE_WIDTH_SHIFT;
    // SWint tile_min_y = tile_bboxi.i32[2] >> SW_CULL_TILE_HEIGHT_SHIFT;
    // SWint tile_max_y = tile_bboxi.i32[3] >> SW_CULL_TILE_HEIGHT_SHIFT;
    SWint tile_row_ndx = (tile_bboxi.i32[2] >> SW_CULL_TILE_HEIGHT_SHIFT) * ctx->tile_w;
    SWint tile_row_end = (tile_bboxi.i32[3] >> SW_CULL_TILE_HEIGHT_SHIFT) * ctx->tile_w;

    subtile_bboxi.vec = _mm128_and_si128(_mm128_add_epi32(px_bboxi, SIMD_SUBTILE_PAD),
                                         SIMD_SUBTILE_PAD_MASK);
    SWint stile_min_x = subtile_bboxi.i32[0];
    SWint stile_min_y = subtile_bboxi.i32[2];
    SWint stile_max_x = subtile_bboxi.i32[1];
    SWint stile_max_y = subtile_bboxi.i32[3];

    const SWint start_px_x[4] = {tile_bboxi.i32[0] + 0 * SW_CULL_SUBTILE_X,
                                 tile_bboxi.i32[0] + 1 * SW_CULL_SUBTILE_X,
                                 tile_bboxi.i32[0] + 2 * SW_CULL_SUBTILE_X,
                                 tile_bboxi.i32[0] + 3 * SW_CULL_SUBTILE_X};
    SWint px_y = tile_bboxi.i32[2];

    const SWfloat z_max = 1 / w_min;

    while (1) {
        SWint px_x[4];
        memcpy(px_x, start_px_x, 4 * sizeof(SWint));
        SWint tile_x = tile_min_x;
        while (1) {
            const SWint tile_ndx = tile_row_ndx + tile_x;
#ifdef SW_CULL_QUICK_MASK
            const SWfloat *z_min0_buf = ztiles[tile_ndx].zmin[0];
#else
#error "Not implemented!"
#endif
            uint32_t z_pass = 0;
            for (SWint j = 0; j < 4; j++) {
                if (z_max >= z_min0_buf[j] && px_x[j] >= stile_min_x &&
                    px_y >= stile_min_y && px_x[j] < stile_max_x && px_y < stile_max_y) {
                    z_pass |= (0xff << j * 8);
                }
            }

            if (z_pass) {
                return 1;
            }

            if (++tile_x >= tile_max_x) {
                break;
            }

            for (SWint j = 0; j < 4; j++) {
                px_x[j] += SW_CULL_TILE_SIZE_X;
            }
        }

        tile_row_ndx += ctx->tile_w;
        if (tile_row_ndx >= tile_row_end) {
            break;
        }

        px_y += SW_CULL_TILE_SIZE_Y;
    }

    return 0;

#undef SIMD_TILE_PAD
#undef SIMD_TILE_PAD_MASK
#undef SIMD_SUBTILE_PAD
#undef SIMD_SUBTILE_PAD_MASK
}

void _swCullCtxDebugDepth_Ref(const SWcull_ctx *ctx, SWfloat *out_depth) {
    const SWztile *ztiles = (SWztile *)ctx->ztiles;

    for (SWint y = 0; y < ctx->h; y++) {
        const SWint ty = y / ctx->tile_size_y;
        for (SWint x = 0; x < ctx->w; x++) {
            const SWint tx = x / SW_CULL_TILE_SIZE_X;

            const SWint tile_ndx = ty * ctx->tile_w + tx;

#if 1 // in case it is transposed
            SWint stx = (x % SW_CULL_TILE_SIZE_X) / SW_CULL_SUBTILE_X;
            SWint sty = (y % SW_CULL_TILE_SIZE_Y) / SW_CULL_SUBTILE_Y;
            SWint subtile_ndx = sty * (SW_CULL_TILE_SIZE_X / SW_CULL_SUBTILE_X) + stx;

            SWint px = (x % SW_CULL_SUBTILE_X);
            SWint py = (y % SW_CULL_SUBTILE_Y);
            SWint bit_ndx = py * SW_CULL_SUBTILE_X + px;

            const uint8_t *cov = (uint8_t *)&ztiles[tile_ndx].mask;
            SWint pix = (cov[subtile_ndx] >> bit_ndx) & 1;
#else
            SWint subtile_ndx = (y % ctx->tile_size_y);
            SWint bit_ndx = (x % SW_CULL_TILE_SIZE_X);

            SWint pix =
                (ztiles[tile_ndx * ctx->tile_size_y + subtile_ndx].mask >> bit_ndx) & 1;
#endif

            SWfloat *depth_val = &out_depth[y * ctx->w + x];
            if (pix) {
                (*depth_val) = ztiles[tile_ndx].zmin[1][subtile_ndx];
            } else {
                (*depth_val) = ztiles[tile_ndx].zmin[0][subtile_ndx];
            }
        }
    }
}

void _swCullCtxClearBuf_Ref(SWcull_ctx *ctx) {
    SWztile *ztiles = (SWztile *)ctx->ztiles;

    for (SWint i = 0; i < ctx->tile_w * ctx->tile_h; i++) {
        ztiles[i].mask = 0;

        ztiles[i].zmin[0][0] = ztiles[i].zmin[0][1] = ztiles[i].zmin[0][2] =
            ztiles[i].zmin[0][3] = -1;

#ifdef SW_CULL_QUICK_MASK
        ztiles[i].zmin[1][0] = ztiles[i].zmin[1][1] = ztiles[i].zmin[1][2] =
            ztiles[i].zmin[1][3] = FLT_MAX;
#else
        ztiles[i].zmin[1][0] = ztiles[i].zmin[1][1] = ztiles[i].zmin[1][2] =
            ztiles[i].zmin[1][3] = 0;
#endif
    }
}

#pragma warning(pop)