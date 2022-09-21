
SWint TRI_FUNC_NAME(SWcull_ctx *ctx, SWint tile_row_ndx, SWint tile_mid_row_ndx,
                    SWint tile_end_row_ndx, SWint bb_width, SWint tri_ndx,
                    SWint slope_tile_delta[], SWint event_start[], SWint slope[],
                    const __mXXX *tri_zmin, const __mXXX *tri_zmax, __mXXX *z0,
                    SWfloat zx, SWfloat zy, SWint flat_bottom) {
#define LEFT_EDGE_BIAS 0
#define RIGHT_EDGE_BIAS 0
#define UPDATE_TILE_EVENTS_Y(i)                                                          \
    tri_event[i] = _mmXXX_add_epi32(tri_event[i], tri_slope_tile_delta[i])

    const __mXXXi tri_slope_tile_delta[3] = {
        _mmXXX_set1_epi32(slope_tile_delta[0 * SIMD_WIDTH + tri_ndx]),
        _mmXXX_set1_epi32(slope_tile_delta[1 * SIMD_WIDTH + tri_ndx]),
        _mmXXX_set1_epi32(slope_tile_delta[2 * SIMD_WIDTH + tri_ndx])};

    __mXXXi tri_event[3] = {
        _mmXXX_add_epi32(
            _mmXXX_set1_epi32(event_start[0 * SIMD_WIDTH + tri_ndx]),
            _mmXXX_mullo_epi32(SIMD_LANE_NDX,
                               _mmXXX_set1_epi32(slope[0 * SIMD_WIDTH + tri_ndx]))),
        _mmXXX_add_epi32(
            _mmXXX_set1_epi32(event_start[1 * SIMD_WIDTH + tri_ndx]),
            _mmXXX_mullo_epi32(SIMD_LANE_NDX,
                               _mmXXX_set1_epi32(slope[1 * SIMD_WIDTH + tri_ndx]))),
        _mmXXX_add_epi32(
            _mmXXX_set1_epi32(event_start[2 * SIMD_WIDTH + tri_ndx]),
            _mmXXX_mullo_epi32(SIMD_LANE_NDX,
                               _mmXXX_set1_epi32(slope[2 * SIMD_WIDTH + tri_ndx])))};

#ifdef TIGHT_TRANVERSAL
    SWint start_delta, end_delta, top_delta, start_event, end_event, top_event;
    start_delta = slope_tile_delta[2 * SIMD_WIDTH + tri_ndx] + LEFT_EDGE_BIAS;
    end_delta = slope_tile_delta[0 * SIMD_WIDTH + tri_ndx] + RIGHT_EDGE_BIAS;
    top_delta = slope_tile_delta[1 * SIMD_WIDTH + tri_ndx] +
                (MID_VTX_RIGHT ? RIGHT_EDGE_BIAS : LEFT_EDGE_BIAS);

    start_event = event_start[2 * SIMD_WIDTH + tri_ndx] + sw_min(0, start_delta);
    end_event = event_start[0 * SIMD_WIDTH + tri_ndx] + sw_max(0, end_delta) +
                (SW_CULL_TILE_SIZE_X << FP_BITS);
#if MID_VTX_RIGHT
    top_event = event_start[1 * SIMD_WIDTH + tri_ndx] + sw_max(0, top_delta) +
                (SW_CULL_TILE_SIZE_X << FP_BITS);
#else // MID_VTX_RIGHT
    top_event = event_start[1 * SIMD_WIDTH + tri_ndx] + sw_min(0, top_delta);
#endif // MID_VTX_RIGHT
#endif // TIGHT_TRANVERSAL

    if (!flat_bottom) {
        SWint tile_stop_ndx = sw_min(tile_end_row_ndx, tile_mid_row_ndx);
        // bottom half of triangle
        while (tile_row_ndx < tile_stop_ndx) {
            SWint start = 0, end = bb_width;
#ifdef TIGHT_TRANVERSAL
            start =
                sw_max(0, sw_min(bb_width - 1,
                                 start_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
            end = sw_min(bb_width, (end_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
            start_event += start_delta;
            end_event += end_delta;
#endif // TIGHT_TRANVERSAL

#ifdef IS_OCCLUDER
            const SWint res = NAME(_swProcessScanlineOccluder_L1R1)(
#else
            const SWint res = NAME(_swProcessScanline_L1R1)(
#endif
                (SWztile *)ctx->ztiles, start, end, 2 /* left_event */,
                0 /* right_event*/, tri_event, tile_row_ndx, tri_zmin, tri_zmax, z0, zx);
#ifndef IS_OCCLUDER
            if (res) {
                return 1;
            }
#else
            (void)res;
#endif

            tile_row_ndx += ctx->tile_w;
            (*z0) = _mmXXX_add_ps(*z0, _mmXXX_set1_ps(zy));
            UPDATE_TILE_EVENTS_Y(0);
            UPDATE_TILE_EVENTS_Y(2);
        }

        // middle part (touched by all three edges)
        if (tile_row_ndx < tile_end_row_ndx) {
            SWint start = 0, end = bb_width;
#ifdef TIGHT_TRANVERSAL
            start =
                sw_max(0, sw_min(bb_width - 1,
                                 start_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
            end = sw_min(bb_width, (end_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));

            end_event = mid_vtx_right ? top_event : end_event;
            end_delta = mid_vtx_right ? top_delta : end_delta;
            start_event = mid_vtx_right ? start_event : top_event;
            start_delta = mid_vtx_right ? start_delta : top_delta;
            start_event += start_delta;
            end_event += end_delta;
#endif // TIGHT_TRANVERSAL

#if MID_VTX_RIGHT
#ifdef IS_OCCLUDER
            const SWint res = NAME(_swProcessScanlineOccluder_L1R2)(
#else
            const SWint res = NAME(_swProcessScanline_L1R2)(
#endif
                (SWztile *)ctx->ztiles, start, end, 2 /* left_event */,
                0 /* right_event*/, tri_event, tile_row_ndx, tri_zmin, tri_zmax, z0, zx);
#else // MID_VTX_RIGHT
#ifdef IS_OCCLUDER
            const SWint res = NAME(_swProcessScanlineOccluder_L2R1)(
#else
            const SWint res = NAME(_swProcessScanline_L2R1)(
#endif
                (SWztile *)ctx->ztiles, start, end, 2 /* left_event */,
                0 /* right_event*/, tri_event, tile_row_ndx, tri_zmin, tri_zmax, z0, zx);
#endif // MID_VTX_RIGHT
#ifndef IS_OCCLUDER
            if (res) {
                return 1;
            }
#else
            (void)res;
#endif

            tile_row_ndx += ctx->tile_w;
        }

        // top half of triangle
        if (tile_row_ndx < tile_end_row_ndx) {
            // move to the next scanline
            (*z0) = _mmXXX_add_ps(*z0, _mmXXX_set1_ps(zy));
            SWint i0 = MID_VTX_RIGHT + 0;
            SWint i1 = MID_VTX_RIGHT + 1;
            UPDATE_TILE_EVENTS_Y(i0);
            UPDATE_TILE_EVENTS_Y(i1);

            while (1) {
                SWint start = 0, end = bb_width;
#ifdef TIGHT_TRANVERSAL
                start = sw_max(
                    0, sw_min(bb_width - 1,
                              start_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                end =
                    sw_min(bb_width, (end_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                start_event += start_delta;
                end_event += end_delta;
#endif // TIGHT_TRANVERSAL

#ifdef IS_OCCLUDER
                const SWint res = NAME(_swProcessScanlineOccluder_L1R1)(
#else
                const SWint res = NAME(_swProcessScanline_L1R1)(
#endif
                    (SWztile *)ctx->ztiles, start, end,
                    MID_VTX_RIGHT + 1 /* left_event */,
                    MID_VTX_RIGHT + 0 /* right_event*/, tri_event, tile_row_ndx, tri_zmin,
                    tri_zmax, z0, zx);

#ifndef IS_OCCLUDER
                if (res) {
                    return 1;
                }
#else
                (void)res;
#endif

                tile_row_ndx += ctx->tile_w;
                if (tile_row_ndx >= tile_end_row_ndx) {
                    break;
                }
                (*z0) = _mmXXX_add_ps(*z0, _mmXXX_set1_ps(zy));
                UPDATE_TILE_EVENTS_Y(i0);
                UPDATE_TILE_EVENTS_Y(i1);
            }
        }
    } else {
#ifdef TIGHT_TRANVERSAL
        end_event = MID_VTX_RIGHT ? top_event : end_event;
        end_delta = MID_VTX_RIGHT ? top_delta : end_delta;
        start_event = MID_VTX_RIGHT ? start_event : top_event;
        start_delta = MID_VTX_RIGHT ? start_delta : top_delta;
#endif // TIGHT_TRANVERSAL

        // top half of triangle
        if (tile_row_ndx < tile_end_row_ndx) {
            SWint i0 = MID_VTX_RIGHT + 0;
            SWint i1 = MID_VTX_RIGHT + 1;

            while (1) {
                SWint start = 0, end = bb_width;
#ifdef TIGHT_TRANVERSAL
                start = sw_max(
                    0, sw_min(bb_width - 1,
                              start_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                end =
                    sw_min(bb_width, (end_event >> (SW_CULL_TILE_WIDTH_SHIFT + FP_BITS)));
                start_event += start_delta;
                end_event += end_delta;
#endif // TIGHT_TRANVERSAL

#ifdef IS_OCCLUDER
                const SWint res = NAME(_swProcessScanlineOccluder_L1R1)(
#else
                const SWint res = NAME(_swProcessScanline_L1R1)(
#endif
                    (SWztile *)ctx->ztiles, start, end,
                    MID_VTX_RIGHT + 1 /* left_event */,
                    MID_VTX_RIGHT + 0 /* right_event*/, tri_event, tile_row_ndx, tri_zmin,
                    tri_zmax, z0, zx);
#ifndef IS_OCCLUDER
                if (res) {
                    return 1;
                }
#else
                (void)res;
#endif

                tile_row_ndx += ctx->tile_w;
                if (tile_row_ndx >= tile_end_row_ndx) {
                    break;
                }
                (*z0) = _mmXXX_add_ps(*z0, _mmXXX_set1_ps(zy));
                UPDATE_TILE_EVENTS_Y(i0);
                UPDATE_TILE_EVENTS_Y(i1);
            }
        }
    }

#ifdef IS_OCCLUDER
    return 1;
#else
    return 0;
#endif

#undef LEFT_EDGE_BIAS
#undef RIGHT_EDGE_BIAS
#undef UPDATE_TILE_EVENTS_Y
}
