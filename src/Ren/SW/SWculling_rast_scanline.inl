
static void SCANLINE_FUNC_NAME(SWuint *coverage, SWint left_offset, SWint right_offset,
                               SWint left_event, SWint right_event,
                               const __mXXXi events[3], SWint tile_ndx) {
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

    SWint tile_ndx_end = tile_ndx + right_offset;
    tile_ndx += left_offset;
    while (1) {
        __mXXXi coverage_mask = _mmXXX_sllv_ones(left[0]);
        for (SWint i = 1; i < LEFT_COUNT; i++) {
            coverage_mask = _mmXXX_and_siXXX(coverage_mask, _mmXXX_sllv_ones(left[i]));
        }
        for (SWint i = 0; i < RIGHT_COUNT; i++) {
            coverage_mask =
                _mmXXX_andnot_siXXX(_mmXXX_sllv_ones(right[i]), coverage_mask);
        }

        __mXXXi *wcov = (__mXXXi *)coverage;
        wcov[tile_ndx] =
            _mmXXX_or_siXXX(wcov[tile_ndx], /*_mm_transpose_epi8*/ (coverage_mask));

        if (++tile_ndx >= tile_ndx_end) {
            break;
        }

        for (SWint i = 0; i < LEFT_COUNT; i++) {
            // saturated sub, does max(x, 0) automatically
            left[i] = _mmXXX_subs_epu16(left[i], _mmXXX_set1_epi32(SW_CULL_TILE_SIZE_X));
        }
        for (SWint i = 0; i < RIGHT_COUNT; i++) {
            right[i] =
                _mmXXX_subs_epu16(right[i], _mmXXX_set1_epi32(SW_CULL_TILE_SIZE_X));
        }
    }
}
