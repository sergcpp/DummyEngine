#include "SWintrin.inl"

#define FP_BITS (19 - SW_CULL_TILE_HEIGHT_SHIFT)

#define SW_CULL_SUBTILE_Y 4

#define PASTER(x, y) x##_##y
#define EVALUATOR(x, y) PASTER(x, y)
#define NAME(fun) EVALUATOR(fun, POSTFIX)

#pragma warning(push)
#pragma warning(disable : 6001) // Using uninitialized memory

//
// Scanline processing
//

typedef struct SWztile {
    union {
        __mXXX vec;
        SWfloat f32[SIMD_WIDTH];
    } zmin[2];
    __mXXXi mask;
} SWztile;

static void NAME(_swUpdateTileQuick)(SWztile *tile, const __mXXXi *coverage_mask,
                                     const __mXXX *z_subtile_min) {
    __mXXXi mask = tile->mask;
    __mXXX *zmin = &tile->zmin->vec;

    __mXXXi rast_mask = *coverage_mask;
    __mXXXi dead_lane = _mmXXX_cmpeq_epi32(rast_mask, _mmXXX_setzero_siXXX());

    // mask subtiles that fail depth test
    dead_lane = _mmXXX_or_siXXX(
        dead_lane, _mmXXX_srai_epi32(
                       _mmXXX_castps_siXXX(_mmXXX_sub_ps(*z_subtile_min, zmin[0])), 31));
    rast_mask = _mmXXX_andnot_siXXX(dead_lane, rast_mask);

    // heuristic to discard layer 1
    __mXXXi covered_lane = _mmXXX_cmpeq_epi32(rast_mask, _mmXXX_set1_epi32(0xffffffff));
    __mXXX diff = _mmXXX_fmsub_ps(zmin[1], _mmXXX_set1_ps(2.0f),
                                  _mmXXX_add_ps(*z_subtile_min, zmin[0]));
    __mXXXi discard_layer_mask = _mmXXX_andnot_siXXX(
        dead_lane,
        _mmXXX_or_siXXX(_mmXXX_srai_epi32(_mmXXX_castps_siXXX(diff), 31), covered_lane));

    // update tile mask
    mask = _mmXXX_or_siXXX(_mmXXX_andnot_siXXX(discard_layer_mask, mask), rast_mask);

    __mXXXi mask_full = _mmXXX_cmpeq_epi32(mask, _mmXXX_set1_epi32(0xffffffff));

    // compute new value of zmin1, there are 4 cases:
    // zmin1 = min(zmin1, z_subtile_min) -> layer is updated
    // zmin1 = z_subtile_min             -> layer is discarded
    // zmin1 = FLT_MAX                   -> layer is fully covered
    // zmin1 unchanged                   -> layer is not updated

    __mXXX op_a =
        _mmXXX_blendv_ps(*z_subtile_min, zmin[1], _mmXXX_castsiXXX_ps(dead_lane));
    __mXXX op_b = _mmXXX_blendv_ps(zmin[1], *z_subtile_min,
                                   _mmXXX_castsiXXX_ps(discard_layer_mask));
    __mXXX zmin1 = _mmXXX_min_ps(op_a, op_b);

    zmin[1] =
        _mmXXX_blendv_ps(zmin1, _mmXXX_set1_ps(FLT_MAX), _mmXXX_castsiXXX_ps(mask_full));
    zmin[0] = _mmXXX_blendv_ps(zmin[0], zmin1, _mmXXX_castsiXXX_ps(mask_full));

    tile->mask = _mmXXX_andnot_siXXX(mask_full, mask);
}

//
// Triangle processing (occludee)
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

//
// Triangle processing (occluder)
//

#define IS_OCCLUDER

#define SCANLINE_FUNC_NAME NAME(_swProcessScanlineOccluder_L1R1)
#define LEFT_COUNT 1
#define RIGHT_COUNT 1
#include "SWculling_rast_scanline.inl"
#undef RIGHT_COUNT
#undef LEFT_COUNT
#undef SCANLINE_FUNC_NAME

#define SCANLINE_FUNC_NAME NAME(_swProcessScanlineOccluder_L2R1)
#define LEFT_COUNT 2
#define RIGHT_COUNT 1
#include "SWculling_rast_scanline.inl"
#undef RIGHT_COUNT
#undef LEFT_COUNT
#undef SCANLINE_FUNC_NAME

#define SCANLINE_FUNC_NAME NAME(_swProcessScanlineOccluder_L1R2)
#define LEFT_COUNT 1
#define RIGHT_COUNT 2
#include "SWculling_rast_scanline.inl"
#undef RIGHT_COUNT
#undef LEFT_COUNT
#undef SCANLINE_FUNC_NAME

#define TRI_FUNC_NAME NAME(_swRasterizeTriangleOccluder_tight_mid_left)
#define TIGHT_TRAVERSAL
#define MID_VTX_RIGHT 0
#include "SWculling_rast_tri.inl"
#undef MID_VTX_RIGHT
#undef TIGHT_TRAVERSAL
#undef TRI_FUNC_NAME

#define TRI_FUNC_NAME NAME(_swRasterizeTriangleOccluder_tight_mid_right)
#define TIGHT_TRAVERSAL
#define MID_VTX_RIGHT 1
#include "SWculling_rast_tri.inl"
#undef MID_VTX_RIGHT
#undef TIGHT_TRAVERSAL
#undef TRI_FUNC_NAME

#define MID_VTX_RIGHT 0
#define TRI_FUNC_NAME NAME(_swRasterizeTriangleOccluder_mid_left)
#include "SWculling_rast_tri.inl"
#undef MID_VTX_RIGHT
#undef TRI_FUNC_NAME

#define MID_VTX_RIGHT 1
#define TRI_FUNC_NAME NAME(_swRasterizeTriangleOccluder_mid_right)
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

void NAME(_swComputeDepthPlane)(const __mXXX vX[3], const __mXXX vY[3],
                                const __mXXX vZ[3], __mXXX *z_px_dx, __mXXX *z_px_dy) {
    // depth plane z(x,y) = z0 + dx * x + dy * y
    const __mXXX x10 = _mmXXX_sub_ps(vX[1], vX[0]);
    const __mXXX x20 = _mmXXX_sub_ps(vX[2], vX[0]);
    const __mXXX y10 = _mmXXX_sub_ps(vY[1], vY[0]);
    const __mXXX y20 = _mmXXX_sub_ps(vY[2], vY[0]);
    const __mXXX z10 = _mmXXX_sub_ps(vZ[1], vZ[0]);
    const __mXXX z20 = _mmXXX_sub_ps(vZ[2], vZ[0]);
    // d = 1 / (x10 * y20 - y10 * x20)
    const __mXXX d =
        _mmXXX_div_ps(_mmXXX_set1_ps(1.0f),
                      _mmXXX_sub_ps(_mmXXX_mul_ps(x10, y20), _mmXXX_mul_ps(y10, x20)));

    // (*z_px_dx) = (z10 * y20 - y10 * z20) * d;
    (*z_px_dx) =
        _mmXXX_mul_ps(_mmXXX_sub_ps(_mmXXX_mul_ps(z10, y20), _mmXXX_mul_ps(y10, z20)), d);
    // (*z_px_dy) = (x10 * z20 - z10 * x20) * d;
    (*z_px_dy) =
        _mmXXX_mul_ps(_mmXXX_sub_ps(_mmXXX_mul_ps(x10, z20), _mmXXX_mul_ps(z10, x20)), d);
}

#if defined(USE_SSE2) || defined(USE_NEON)
#define SIMD_SUB_TILE_COL_OFFSET_I                                                       \
    _mmXXX_setr_epi32(0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3)
#define SIMD_SUB_TILE_COL_OFFSET_F                                                       \
    _mmXXX_setr_ps(0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3)
#define SIMD_SUB_TILE_ROW_OFFSET_I _mmXXX_setzero_siXXX()
#define SIMD_SUB_TILE_ROW_OFFSET_F _mmXXX_setzero_ps()
#elif defined(USE_AVX2)
#define SIMD_SUB_TILE_COL_OFFSET_I                                                       \
    _mm256_setr_epi32(0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2,                       \
                      SW_CULL_SUBTILE_X * 3, 0, SW_CULL_SUBTILE_X,                       \
                      SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3)
#define SIMD_SUB_TILE_COL_OFFSET_F                                                       \
    _mm256_setr_ps(0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3,   \
                   0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3)
#define SIMD_SUB_TILE_ROW_OFFSET_I                                                       \
    _mm256_setr_epi32(0, 0, 0, 0, SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y,                  \
                      SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y)
#define SIMD_SUB_TILE_ROW_OFFSET_F                                                       \
    _mm256_setr_ps(0, 0, 0, 0, SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y,  \
                   SW_CULL_SUBTILE_Y)
#elif defined(USE_AVX512)
#define SIMD_SUB_TILE_COL_OFFSET_I                                                       \
    _mm512_setr_epi32(                                                                   \
        0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3, 0,           \
        SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3, 0,              \
        SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3, 0,              \
        SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3)
#define SIMD_SUB_TILE_COL_OFFSET_F                                                       \
    _mm512_setr_ps(0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3,   \
                   0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3,   \
                   0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3,   \
                   0, SW_CULL_SUBTILE_X, SW_CULL_SUBTILE_X * 2, SW_CULL_SUBTILE_X * 3)
#define SIMD_SUB_TILE_ROW_OFFSET_I                                                       \
    _mm512_setr_epi32(                                                                   \
        0, 0, 0, 0, SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y,             \
        SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y * 2, SW_CULL_SUBTILE_Y * 2,                 \
        SW_CULL_SUBTILE_Y * 2, SW_CULL_SUBTILE_Y * 2, SW_CULL_SUBTILE_Y * 3,             \
        SW_CULL_SUBTILE_Y * 3, SW_CULL_SUBTILE_Y * 3, SW_CULL_SUBTILE_Y * 3)
#define SIMD_SUB_TILE_ROW_OFFSET_F                                                       \
    _mm512_setr_ps(0, 0, 0, 0, SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y,  \
                   SW_CULL_SUBTILE_Y, SW_CULL_SUBTILE_Y * 2, SW_CULL_SUBTILE_Y * 2,      \
                   SW_CULL_SUBTILE_Y * 2, SW_CULL_SUBTILE_Y * 2, SW_CULL_SUBTILE_Y * 3,  \
                   SW_CULL_SUBTILE_Y * 3, SW_CULL_SUBTILE_Y * 3, SW_CULL_SUBTILE_Y * 3)
#endif

SWint NAME(_swProcessTriangleBatch)(SWcull_ctx *ctx, __mXXX vX[3], __mXXX vY[3],
                                    __mXXX vZ[3], SWuint tri_mask, SWint is_occluder) {
    // find triangle bounds
    __mXXXi bb_px_min_x =
        _mmXXX_cvttps_epi32(_mmXXX_min_ps(vX[0], _mmXXX_min_ps(vX[1], vX[2])));
    __mXXXi bb_px_min_y =
        _mmXXX_cvttps_epi32(_mmXXX_min_ps(vY[0], _mmXXX_min_ps(vY[1], vY[2])));
    __mXXXi bb_px_max_x =
        _mmXXX_cvttps_epi32(_mmXXX_max_ps(vX[0], _mmXXX_max_ps(vX[1], vX[2])));
    __mXXXi bb_px_max_y =
        _mmXXX_cvttps_epi32(_mmXXX_max_ps(vY[0], _mmXXX_max_ps(vY[1], vY[2])));

    // clamp to frame bounds
    bb_px_min_x = _mmXXX_max_epi32(bb_px_min_x, _mmXXX_set1_epi32(0));
    bb_px_max_x = _mmXXX_min_epi32(bb_px_max_x,
                                   _mmXXX_set1_epi32(ctx->tile_w * SW_CULL_TILE_SIZE_X));
    bb_px_min_y = _mmXXX_max_epi32(bb_px_min_y, _mmXXX_set1_epi32(0));
    bb_px_max_y = _mmXXX_min_epi32(bb_px_max_y,
                                   _mmXXX_set1_epi32(ctx->tile_h * SW_CULL_TILE_SIZE_Y));

    // snap to tiles (min % TILE_SIZE_, (max + TILE_SIZE_ - 1) % TILE_SIZE_)
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
        return 0;
    }

    union {
        __mXXX vec;
        SWfloat f32[SIMD_WIDTH];
    } z_px_dx, z_px_dy, z_plane_offset, z_tile_dx, z_tile_dy, zmin, zmax;

    NAME(_swComputeDepthPlane)(vX, vY, vZ, &z_px_dx.vec, &z_px_dy.vec);

    __mXXX bb_min_v0X = _mmXXX_sub_ps(_mmXXX_cvtepi32_ps(bb_px_min_x), vX[0]);
    __mXXX bb_min_v0Y = _mmXXX_sub_ps(_mmXXX_cvtepi32_ps(bb_px_min_y), vY[0]);
    z_plane_offset.vec = _mmXXX_fmadd_ps(z_px_dx.vec, bb_min_v0X,
                                         _mmXXX_fmadd_ps(z_px_dy.vec, bb_min_v0Y, vZ[0]));
    z_tile_dx.vec = _mmXXX_mul_ps(z_px_dx.vec, _mmXXX_set1_ps(SW_CULL_TILE_SIZE_X));
    z_tile_dy.vec = _mmXXX_mul_ps(z_px_dy.vec, _mmXXX_set1_ps(SW_CULL_TILE_SIZE_Y));

    if (is_occluder) {
        z_plane_offset.vec = _mmXXX_add_ps(
            z_plane_offset.vec,
            _mmXXX_min_ps(_mmXXX_setzero_ps(),
                          _mmXXX_mul_ps(z_px_dx.vec, _mmXXX_set1_ps(SW_CULL_SUBTILE_X))));
        z_plane_offset.vec = _mmXXX_add_ps(
            z_plane_offset.vec,
            _mmXXX_min_ps(_mmXXX_setzero_ps(),
                          _mmXXX_mul_ps(z_px_dy.vec, _mmXXX_set1_ps(SW_CULL_SUBTILE_Y))));
    } else {
        z_plane_offset.vec = _mmXXX_add_ps(
            z_plane_offset.vec,
            _mmXXX_max_ps(_mmXXX_setzero_ps(),
                          _mmXXX_mul_ps(z_px_dx.vec, _mmXXX_set1_ps(SW_CULL_SUBTILE_X))));
        z_plane_offset.vec = _mmXXX_add_ps(
            z_plane_offset.vec,
            _mmXXX_max_ps(_mmXXX_setzero_ps(),
                          _mmXXX_mul_ps(z_px_dy.vec, _mmXXX_set1_ps(SW_CULL_SUBTILE_Y))));
    }

    zmin.vec = _mmXXX_min_ps(vZ[0], _mmXXX_min_ps(vZ[1], vZ[2]));
    zmax.vec = _mmXXX_max_ps(vZ[0], _mmXXX_max_ps(vZ[1], vZ[2]));

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
        _mmXXX_set1_ps((SWfloat)ctx->w + 2.0f * (1.0f + 1.0f));
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
        bb_tile_min_x, _mmXXX_mullo_epi32(bb_tile_min_y, _mmXXX_set1_epi32(ctx->tile_w)));
    bb_top_ndx.vec = _mmXXX_add_epi32(
        bb_tile_min_x,
        _mmXXX_mullo_epi32(_mmXXX_add_epi32(bb_tile_min_y, bb_tile_size_y.vec),
                           _mmXXX_set1_epi32(ctx->tile_w)));
    bb_mid_ndx.vec = _mmXXX_add_epi32(
        bb_tile_min_x, _mmXXX_mullo_epi32(mid_tile_y, _mmXXX_set1_epi32(ctx->tile_w)));

    while (tri_mask) {
        const SWint tri_ndx = _swGetFirstBit(tri_mask);
        tri_mask &= tri_mask - 1;

        const __mXXX tri_zmin = _mmXXX_set1_ps(zmin.f32[tri_ndx]);
        const __mXXX tri_zmax = _mmXXX_set1_ps(zmax.f32[tri_ndx]);

        __mXXX z0 = _mmXXX_fmadd_ps(
            _mmXXX_set1_ps(z_px_dx.f32[tri_ndx]), SIMD_SUB_TILE_COL_OFFSET_F,
            _mmXXX_fmadd_ps(_mmXXX_set1_ps(z_px_dy.f32[tri_ndx]),
                            SIMD_SUB_TILE_ROW_OFFSET_F,
                            _mmXXX_set1_ps(z_plane_offset.f32[tri_ndx])));
        const SWfloat zx = z_tile_dx.f32[tri_ndx];
        const SWfloat zy = z_tile_dy.f32[tri_ndx];

        const SWint tri_bb_width = bb_tile_size_x.i32[tri_ndx];
        const SWint tri_bb_height = bb_tile_size_y.i32[tri_ndx];

        const SWint tile_row_ndx = bb_bottom_ndx.i32[tri_ndx];
        const SWint tile_mid_row_ndx = bb_mid_ndx.i32[tri_ndx];
        const SWint tile_end_row_ndx = bb_top_ndx.i32[tri_ndx];

        const SWint tri_mid_vtx_right = (mid_vtx_right >> tri_ndx) & 1;
        const SWint tri_flat_bottom = (flat_bottom >> tri_ndx) & 1;

        SWint res;
        if (tri_bb_width > 3 && tri_bb_height > 3) {
            if (tri_mid_vtx_right) {
                if (is_occluder) {
                    res = NAME(_swRasterizeTriangleOccluder_tight_mid_right)(
                        ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx,
                        tri_bb_width, tri_ndx, slope_tile_delta[0].i32,
                        event_start[0].i32, slope_fp[0].i32, &tri_zmin, &tri_zmax, &z0,
                        zx, zy, tri_flat_bottom);
                } else {
                    res = NAME(_swRasterizeTriangle_tight_mid_right)(
                        ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx,
                        tri_bb_width, tri_ndx, slope_tile_delta[0].i32,
                        event_start[0].i32, slope_fp[0].i32, &tri_zmin, &tri_zmax, &z0,
                        zx, zy, tri_flat_bottom);
                }
            } else {
                if (is_occluder) {
                    res = NAME(_swRasterizeTriangleOccluder_tight_mid_left)(
                        ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx,
                        tri_bb_width, tri_ndx, slope_tile_delta[0].i32,
                        event_start[0].i32, slope_fp[0].i32, &tri_zmin, &tri_zmax, &z0,
                        zx, zy, tri_flat_bottom);
                } else {
                    res = NAME(_swRasterizeTriangle_tight_mid_left)(
                        ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx,
                        tri_bb_width, tri_ndx, slope_tile_delta[0].i32,
                        event_start[0].i32, slope_fp[0].i32, &tri_zmin, &tri_zmax, &z0,
                        zx, zy, tri_flat_bottom);
                }
            }
        } else {
            if (tri_mid_vtx_right) {
                if (is_occluder) {
                    res = NAME(_swRasterizeTriangleOccluder_mid_right)(
                        ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx,
                        tri_bb_width, tri_ndx, slope_tile_delta[0].i32,
                        event_start[0].i32, slope_fp[0].i32, &tri_zmin, &tri_zmax, &z0,
                        zx, zy, tri_flat_bottom);
                } else {
                    res = NAME(_swRasterizeTriangle_mid_right)(
                        ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx,
                        tri_bb_width, tri_ndx, slope_tile_delta[0].i32,
                        event_start[0].i32, slope_fp[0].i32, &tri_zmin, &tri_zmax, &z0,
                        zx, zy, tri_flat_bottom);
                }
            } else {
                if (is_occluder) {
                    res = NAME(_swRasterizeTriangleOccluder_mid_left)(
                        ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx,
                        tri_bb_width, tri_ndx, slope_tile_delta[0].i32,
                        event_start[0].i32, slope_fp[0].i32, &tri_zmin, &tri_zmax, &z0,
                        zx, zy, tri_flat_bottom);
                } else {
                    res = NAME(_swRasterizeTriangle_mid_left)(
                        ctx, tile_row_ndx, tile_mid_row_ndx, tile_end_row_ndx,
                        tri_bb_width, tri_ndx, slope_tile_delta[0].i32,
                        event_start[0].i32, slope_fp[0].i32, &tri_zmin, &tri_zmax, &z0,
                        zx, zy, tri_flat_bottom);
                }
            }
        }

        if (res && !is_occluder) {
            return 1;
        }
    }

    return is_occluder;
}

SWint _swClipPolygon(const __m128 in_vtx[], const SWint in_vtx_count, const __m128 plane,
                     __m128 out_vtx[]);

#define SW_MAX_CLIPPED (8 * SIMD_WIDTH)

SWint NAME(_swProcessTrianglesIndexed)(SWcull_ctx *ctx, const void *attribs,
                                       const SWuint *indices, const SWuint stride,
                                       const SWuint index_count, const SWfloat *xform,
                                       const SWint is_occluder) {
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

        const SWint res = NAME(_swProcessTriangleBatch)(
            ctx, &vX[0].vec, &vY[0].vec, &vW[0].vec, tri_mask, is_occluder);
        if (res && !is_occluder) {
            return 1;
        }
    }

    return is_occluder;
}

SWint NAME(_swCullCtxTestRect)(const SWcull_ctx *ctx, const SWfloat p_min[2],
                               const SWfloat p_max[3], const SWfloat w_min) {
#define SIMD_TILE_PAD                                                                    \
    _mm128_setr_epi32(0, SW_CULL_TILE_SIZE_X - 1, 0, SW_CULL_TILE_SIZE_Y - 1)
#define SIMD_TILE_PAD_MASK                                                               \
    _mm128_setr_epi32(~(SW_CULL_TILE_SIZE_X - 1), ~(SW_CULL_TILE_SIZE_X - 1),            \
                      ~(SW_CULL_TILE_SIZE_Y - 1), ~(SW_CULL_TILE_SIZE_Y - 1))
#define SIMD_SUBTILE_PAD                                                                 \
    _mm128_setr_epi32(0, SW_CULL_SUBTILE_X - 1, 0, SW_CULL_SUBTILE_Y - 1)
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
    //SWint tile_min_y = tile_bboxi.i32[2] >> SW_CULL_TILE_HEIGHT_SHIFT;
    //SWint tile_max_y = tile_bboxi.i32[3] >> SW_CULL_TILE_HEIGHT_SHIFT;
    SWint tile_row_ndx = (tile_bboxi.i32[2] >> SW_CULL_TILE_HEIGHT_SHIFT) * ctx->tile_w;
    SWint tile_row_end = (tile_bboxi.i32[3] >> SW_CULL_TILE_HEIGHT_SHIFT) * ctx->tile_w;

    subtile_bboxi.vec = _mm128_and_si128(_mm128_add_epi32(px_bboxi, SIMD_SUBTILE_PAD),
                                         SIMD_SUBTILE_PAD_MASK);
    __mXXXi stile_min_x = _mmXXX_set1_epi32(subtile_bboxi.i32[0] - 1); // no >= for epi32
    __mXXXi stile_max_x = _mmXXX_set1_epi32(subtile_bboxi.i32[1] - 1); // so we use -1
    __mXXXi stile_min_y = _mmXXX_set1_epi32(subtile_bboxi.i32[2]);
    __mXXXi stile_max_y = _mmXXX_set1_epi32(subtile_bboxi.i32[3]);

    __mXXXi start_px_x = _mmXXX_add_epi32(_mmXXX_set1_epi32(tile_bboxi.i32[0]),
                                          SIMD_SUB_TILE_COL_OFFSET_I);
    __mXXXi px_y = _mmXXX_add_epi32(_mmXXX_set1_epi32(tile_bboxi.i32[2]),
                                    SIMD_SUB_TILE_ROW_OFFSET_I);

    __mXXX z_max = _mmXXX_div_ps(_mmXXX_set1_ps(1), _mmXXX_set1_ps(w_min));

    while (1) {
        __mXXXi px_x = start_px_x;
        SWint tile_x = tile_min_x;
        while (1) {
            SWint tile_ndx = tile_row_ndx + tile_x;
#ifdef SW_CULL_QUICK_MASK
            const __mXXX z_min0_buf = ztiles[tile_ndx].zmin[0].vec;
#else
#error "Not implemented!"
#endif
            __mXXXi z_pass = _mmXXX_castps_siXXX(_mmXXX_cmpge_ps(z_max, z_min0_buf));

            __mXXXi bbox_pass_min =
                _mmXXX_and_siXXX(_mmXXX_cmpgt_epi32(px_x, stile_min_x),
                                 _mmXXX_cmpgt_epi32(px_y, stile_min_y));
            __mXXXi bbox_pass_max =
                _mmXXX_and_siXXX(_mmXXX_cmpgt_epi32(stile_max_x, px_x),
                                 _mmXXX_cmpgt_epi32(stile_max_y, px_y));
            __mXXXi bbox_pass = _mmXXX_and_siXXX(bbox_pass_min, bbox_pass_max);
            z_pass = _mmXXX_and_siXXX(z_pass, bbox_pass);

            if (!_mmXXX_testz_siXXX(z_pass, z_pass)) {
                return 1;
            }

            if (++tile_x >= tile_max_x) {
                break;
            }

            px_x = _mmXXX_add_epi32(px_x, _mmXXX_set1_epi32(SW_CULL_TILE_SIZE_X));
        }

        tile_row_ndx += ctx->tile_w;
        if (tile_row_ndx >= tile_row_end) {
            break;
        }

        px_y = _mmXXX_add_epi32(px_y, _mmXXX_set1_epi32(SW_CULL_TILE_SIZE_Y));
    }

    return 0;

#undef SIMD_TILE_PAD
#undef SIMD_TILE_PAD_MASK
#undef SIMD_SUBTILE_PAD
#undef SIMD_SUBTILE_PAD_MASK
}

void NAME(_swCullCtxDebugDepth)(const SWcull_ctx *ctx, SWfloat *out_depth) {
    const SWztile *ztiles = (SWztile *)ctx->ztiles;

    for (SWint y = 0; y < ctx->h; y++) {
        SWint ty = y / ctx->tile_size_y;
        for (SWint x = 0; x < ctx->w; x++) {
            SWint tx = x / SW_CULL_TILE_SIZE_X;

            SWint tile_ndx = ty * ctx->tile_w + tx;

#if 1 // in case it is transposed (needed later)
            SWint stx = (x % SW_CULL_TILE_SIZE_X) / SW_CULL_SUBTILE_X;
            SWint sty = (y % SW_CULL_TILE_SIZE_Y) / SW_CULL_SUBTILE_Y;
            SWint subtile_ndx = sty * (SW_CULL_TILE_SIZE_X / SW_CULL_SUBTILE_X) + stx;

            SWint px = (x % SW_CULL_SUBTILE_X);
            SWint py = (y % SW_CULL_SUBTILE_Y);
            SWint bit_ndx = py * SW_CULL_SUBTILE_X + px;

            const uint32_t *cov = (uint32_t *)&ztiles[tile_ndx].mask;
            SWint pix = (cov[subtile_ndx] >> bit_ndx) & 1;
#else
            SWint subtile_ndx = (y % ctx->tile_size_y);
            SWint bit_ndx = (x % SW_CULL_TILE_SIZE_X);

            SWint pix =
                (ctx->coverage[tile_ndx * ctx->tile_size_y + subtile_ndx] >> bit_ndx) & 1;
#endif

            SWfloat *depth_val = &out_depth[y * ctx->w + x];
            if (pix) {
                (*depth_val) = ztiles[tile_ndx].zmin[1].f32[subtile_ndx];
                //(*depth_val) = 1;
            } else {
                (*depth_val) = ztiles[tile_ndx].zmin[0].f32[subtile_ndx];
                //(*depth_val) = 0;
            }
        }
    }
}

void NAME(_swCullCtxClearBuf)(SWcull_ctx *ctx) {
    SWztile *ztiles = (SWztile *)ctx->ztiles;

    for (SWint i = 0; i < ctx->tile_w * ctx->tile_h; i++) {
        ztiles[i].mask = _mmXXX_setzero_siXXX();

        ztiles[i].zmin[0].vec = _mmXXX_set1_ps(-1.0f);

#ifdef SW_CULL_QUICK_MASK
        ztiles[i].zmin[1].vec = _mmXXX_set1_ps(FLT_MAX);
#else
        ztiles[i].zmin[1].vec = _mmXXX_setzero_ps();
#endif
    }
}

#pragma warning(pop)