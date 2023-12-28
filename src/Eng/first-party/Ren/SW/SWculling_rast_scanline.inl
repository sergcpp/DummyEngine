
#define SIMD_ALL_LANES_MASK ((1 << SIMD_WIDTH) - 1)

static SWint SCANLINE_FUNC_NAME(SWztile *tiles, SWint left_offset, SWint right_offset,
                                SWint left_event, SWint right_event,
                                const __mXXXi events[3], SWint tile_ndx,
                                const __mXXX *tri_zmin, const __mXXX *tri_zmax,
                                const __mXXX *_z0, SWfloat zx) {
    SWint event_offset = (left_offset << SW_CULL_TILE_WIDTH_SHIFT);

    __mXXXi left[LEFT_COUNT], right[RIGHT_COUNT];
    for (SWint i = 0; i < LEFT_COUNT; i++) {
        left[i] = _mmXXX_max_epi32(
            _mmXXX_sub_epi32(_mmXXX_srai_epi32(events[left_event - i], FP_BITS),
                             _mmXXX_set1_epi32(event_offset)),
            _mmXXX_set1_epi32(0));
    }
    for (SWint i = 0; i < RIGHT_COUNT; i++) {
        right[i] = _mmXXX_max_epi32(
            _mmXXX_sub_epi32(_mmXXX_srai_epi32(events[right_event + i], FP_BITS),
                             _mmXXX_set1_epi32(event_offset)),
            _mmXXX_set1_epi32(0));
    }

    __mXXX z0 = _mmXXX_add_ps(*_z0, _mmXXX_set1_ps(zx * left_offset));

    SWint tile_ndx_end = tile_ndx + right_offset;
    tile_ndx += left_offset;
    while (1) {
#ifdef SW_CULL_QUICK_MASK
        __mXXX zmin0_buf = tiles[tile_ndx].zmin[0].vec;
#else
        __mXXX zmin0_buf;
//#error "Not implemented!"
#endif

        __mXXX dist0 = _mmXXX_sub_ps(*tri_zmax, zmin0_buf);
        if (_mmXXX_movemask_ps(dist0) != SIMD_ALL_LANES_MASK) {
            __mXXXi coverage_mask = _mmXXX_sllv_ones(left[0]);
            for (SWint i = 1; i < LEFT_COUNT; i++) {
                coverage_mask =
                    _mmXXX_and_siXXX(coverage_mask, _mmXXX_sllv_ones(left[i]));
            }
            for (SWint i = 0; i < RIGHT_COUNT; i++) {
                coverage_mask =
                    _mmXXX_andnot_siXXX(_mmXXX_sllv_ones(right[i]), coverage_mask);
            }

            // rearrange to 8x4 tiles
            coverage_mask = _mmXXX_transpose_epi8(coverage_mask);

#ifdef IS_OCCLUDER
            __mXXX z_subtile_min = _mmXXX_max_ps(z0, *tri_zmin);
#ifdef SW_CULL_QUICK_MASK
            NAME(_swUpdateTileQuick)
            (&tiles[tile_ndx], &coverage_mask, &z_subtile_min);
#else

#endif
#else // occludee case
            __mXXX z_subtile_max = _mmXXX_min_ps(z0, *tri_zmax);
            __mXXXi z_pass =
                _mmXXX_castps_siXXX(_mmXXX_cmpge_ps(z_subtile_max, zmin0_buf));

            __mXXXi dead_lane = _mmXXX_cmpeq_epi32(coverage_mask, _mmXXX_setzero_siXXX());
            z_pass = _mmXXX_andnot_siXXX(dead_lane, z_pass);

            if (!_mmXXX_testz_siXXX(z_pass, z_pass)) {
                return 1;
            }
#endif
        }

        if (++tile_ndx >= tile_ndx_end) {
            break;
        }
        z0 = _mmXXX_add_ps(z0, _mmXXX_set1_ps(zx));
        for (SWint i = 0; i < LEFT_COUNT; i++) {
            // saturated sub, does max(x, 0) automatically
            left[i] = _mmXXX_subs_epu16(left[i], _mmXXX_set1_epi32(SW_CULL_TILE_SIZE_X));
        }
        for (SWint i = 0; i < RIGHT_COUNT; i++) {
            right[i] =
                _mmXXX_subs_epu16(right[i], _mmXXX_set1_epi32(SW_CULL_TILE_SIZE_X));
        }
    }

#ifdef IS_OCCLUDER
    return 1;
#else
    return 0;
#endif
}
