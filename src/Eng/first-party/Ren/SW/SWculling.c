#include "SWculling.h"

#include "SWalloc.h"
#include "SWintrin.inl"

SWint _swProcessTrianglesIndexed_Ref(SWcull_ctx *ctx, const void *attribs, const SWuint *indices, SWuint stride,
                                     SWuint index_count, const SWfloat *xform, SWint is_occluder);
SWint _swCullCtxTestRect_Ref(const SWcull_ctx *ctx, const SWfloat p_min[2], const SWfloat p_max[3],
                             const SWfloat w_min);
void _swCullCtxClearBuf_Ref(SWcull_ctx *ctx);
void _swCullCtxDebugDepth_Ref(const SWcull_ctx *ctx, SWfloat *out_depth);

#if defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
SWint _swProcessTrianglesIndexed_NEON(SWcull_ctx *ctx, const void *attribs, const SWuint *indices, SWuint stride,
                                      SWuint index_count, const SWfloat *xform, SWint is_occluder);
SWint _swCullCtxTestRect_NEON(const SWcull_ctx *ctx, const SWfloat p_min[2], const SWfloat p_max[3],
                              const SWfloat w_min);
void _swCullCtxClearBuf_NEON(SWcull_ctx *ctx);
void _swCullCtxDebugDepth_NEON(const SWcull_ctx *ctx, SWfloat *out_depth);
#else // defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
SWint _swProcessTrianglesIndexed_SSE2(SWcull_ctx *ctx, const void *attribs, const SWuint *indices, SWuint stride,
                                      SWuint index_count, const SWfloat *xform, SWint is_occluder);
SWint _swProcessTrianglesIndexed_AVX2(SWcull_ctx *ctx, const void *attribs, const SWuint *indices, SWuint stride,
                                      SWuint index_count, const SWfloat *xform, SWint is_occluder);

SWint _swCullCtxTestRect_SSE2(const SWcull_ctx *ctx, const SWfloat p_min[2], const SWfloat p_max[3],
                              const SWfloat w_min);
SWint _swCullCtxTestRect_AVX2(const SWcull_ctx *ctx, const SWfloat p_min[2], const SWfloat p_max[3],
                              const SWfloat w_min);

void _swCullCtxClearBuf_SSE2(SWcull_ctx *ctx);
void _swCullCtxClearBuf_AVX2(SWcull_ctx *ctx);

void _swCullCtxDebugDepth_SSE2(const SWcull_ctx *ctx, SWfloat *out_depth);
void _swCullCtxDebugDepth_AVX2(const SWcull_ctx *ctx, SWfloat *out_depth);

#if !defined(_MSC_VER) || _MSC_VER > 1916
SWint _swProcessTrianglesIndexed_AVX512(SWcull_ctx *ctx, const void *attribs, const SWuint *indices, SWuint stride,
                                        SWuint index_count, const SWfloat *xform, SWint is_occluder);
SWint _swCullCtxTestRect_AVX512(const SWcull_ctx *ctx, const SWfloat p_min[2], const SWfloat p_max[3],
                                const SWfloat w_min);
void _swCullCtxClearBuf_AVX512(SWcull_ctx *ctx);
void _swCullCtxDebugDepth_AVX512(const SWcull_ctx *ctx, SWfloat *out_depth);
#endif
#endif // defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)

void swCullCtxInit(SWcull_ctx *ctx, const SWint w, const SWint h, SWfloat near_clip) {
    swCPUInfoInit(&ctx->cpu_info);

    ctx->ztiles = NULL;
    swCullCtxResize(ctx, w, h, near_clip);

    swCullCtxClear(ctx);
}

void swCullCtxDestroy(SWcull_ctx *ctx) {
    swCPUInfoDestroy(&ctx->cpu_info);
    sw_aligned_free(ctx->ztiles);
    memset(ctx, 0, sizeof(SWcull_ctx));
}

void swCullCtxResize(SWcull_ctx *ctx, const SWint w, const SWint h, SWfloat near_clip) {
    if (ctx->w == w && ctx->h == h && ctx->near_clip == near_clip) {
        return;
    }

    ctx->w = w;
    ctx->h = h;

    ctx->half_w = (SWfloat)w / 2;
    ctx->half_h = (SWfloat)h / 2;

#if defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    ctx->tile_size_y = 4;
    ctx->subtile_size_y = 4;
    ctx->tri_indexed_proc = (SWCullTrianglesIndexedProcType)&_swProcessTrianglesIndexed_NEON;
    ctx->test_rect_proc = &_swCullCtxTestRect_NEON;
    ctx->clear_buf_proc = &_swCullCtxClearBuf_NEON;
    ctx->debug_depth_proc = (SWCullDebugDepthProcType)&_swCullCtxDebugDepth_NEON;
#else
#if !defined(_MSC_VER) || _MSC_VER > 1916
    if (ctx->cpu_info.avx512_supported) {
        ctx->tile_size_y = 16;
        ctx->subtile_size_y = 4;
        ctx->tri_indexed_proc = (SWCullTrianglesIndexedProcType)&_swProcessTrianglesIndexed_AVX512;
        ctx->test_rect_proc = &_swCullCtxTestRect_AVX512;
        ctx->clear_buf_proc = &_swCullCtxClearBuf_AVX512;
        ctx->debug_depth_proc = (SWCullDebugDepthProcType)&_swCullCtxDebugDepth_AVX512;
    } else
#endif
	if (ctx->cpu_info.avx2_supported) {
        ctx->tile_size_y = 8;
        ctx->subtile_size_y = 4;
        ctx->tri_indexed_proc = (SWCullTrianglesIndexedProcType)&_swProcessTrianglesIndexed_AVX2;
        ctx->test_rect_proc = &_swCullCtxTestRect_AVX2;
        ctx->clear_buf_proc = &_swCullCtxClearBuf_AVX2;
        ctx->debug_depth_proc = (SWCullDebugDepthProcType)&_swCullCtxDebugDepth_AVX2;
    } else if (ctx->cpu_info.sse2_supported) {
        ctx->tile_size_y = 4;
        ctx->subtile_size_y = 4;
        ctx->tri_indexed_proc = (SWCullTrianglesIndexedProcType)&_swProcessTrianglesIndexed_SSE2;
        ctx->test_rect_proc = &_swCullCtxTestRect_SSE2;
        ctx->clear_buf_proc = &_swCullCtxClearBuf_SSE2;
        ctx->debug_depth_proc = (SWCullDebugDepthProcType)&_swCullCtxDebugDepth_SSE2;
    } else
#endif
    {
        ctx->tile_size_y = 1;
        ctx->subtile_size_y = 1;
        ctx->tri_indexed_proc = (SWCullTrianglesIndexedProcType)&_swProcessTrianglesIndexed_Ref;
        ctx->test_rect_proc = &_swCullCtxTestRect_Ref;
        ctx->clear_buf_proc = &_swCullCtxClearBuf_Ref;
        ctx->debug_depth_proc = (SWCullDebugDepthProcType)&_swCullCtxDebugDepth_Ref;
    }

    assert((w % SW_CULL_SUBTILE_X == 0) && (h % ctx->subtile_size_y == 0));

    ctx->tile_w = (w + (SW_CULL_TILE_SIZE_X - 1)) / SW_CULL_TILE_SIZE_X;
    ctx->tile_h = (h + (ctx->tile_size_y - 1)) / ctx->tile_size_y;

    const int tile_size = SW_CULL_TILE_SIZE_X * ctx->tile_size_y / 8 + 2 * sizeof(float) *
                                                                           (SW_CULL_TILE_SIZE_X / SW_CULL_SUBTILE_X) *
                                                                           (ctx->tile_size_y / ctx->subtile_size_y);

    ctx->ztiles_mem_size = ctx->tile_w * ctx->tile_h * tile_size;
    sw_aligned_free(ctx->ztiles);
    ctx->ztiles = sw_aligned_malloc(ctx->ztiles_mem_size, 64);

    assert((uintptr_t)ctx->size_ivec4 % 16 == 0);
    __m128i *size_ivec4 = (__m128i *)ctx->size_ivec4;
    (*size_ivec4) = _mm128_setr_epi32(ctx->w, ctx->w, ctx->h, ctx->h);

    assert((uintptr_t)ctx->half_size_vec4 % 16 == 0);
    __m128 *half_size = (__m128 *)ctx->half_size_vec4;
    (*half_size) = _mm128_setr_ps(ctx->half_w, ctx->half_w, ctx->half_h, ctx->half_h);

    const SWfloat pad_w = ((SWfloat)2) / ctx->w;
    const SWfloat pad_h = ((SWfloat)2) / ctx->h;

    assert((uintptr_t)ctx->clip_planes % 16 == 0);
    __m128 *clip_planes = (__m128 *)ctx->clip_planes;
    clip_planes[0] = _mm128_setr_ps(1.0f - pad_w, 0.0f, 1.0f, 0.0f);
    clip_planes[1] = _mm128_setr_ps(-1.0f + pad_w, 0.0f, 1.0f, 0.0f);
    clip_planes[2] = _mm128_setr_ps(0.0f, -1.0f + pad_h, 1.0f, 0.0f);
    clip_planes[3] = _mm128_setr_ps(0.0f, 1.0f - pad_h, 1.0f, 0.0f);
    clip_planes[4] = _mm128_setr_ps(0.0f, 0.0f, 1.0f, -near_clip);

    ctx->near_clip = near_clip;
}

void swCullCtxClear(SWcull_ctx *ctx) { (*ctx->clear_buf_proc)(ctx); }

void swCullCtxSubmitCullSurfs(SWcull_ctx *ctx, SWcull_surf *surfs, const SWuint count) {
    for (SWuint i = 0; i < count; i++) {
        SWcull_surf *s = &surfs[i];

        if (s->indices) {
            if (s->prim_type == SW_TRIANGLES) {
                if (s->index_type == SW_UNSIGNED_INT) {
                    s->visible = (*ctx->tri_indexed_proc)(ctx, s->attribs, (const SWuint *)s->indices, s->stride,
                                                          s->count, s->xform, (s->type == SW_OCCLUDER));
                } else {
                    assert(0);
                }
            }
        } else {
        }
    }
}

SWint swCullCtxTestRect(SWcull_ctx *ctx, const SWfloat p_min[2], const SWfloat p_max[3], const SWfloat w_min) {
    return (*ctx->test_rect_proc)(ctx, p_min, p_max, w_min);
}

SWint _swClipPolygon(const __m128 in_vtx[], const SWint in_vtx_count, const __m128 plane, __m128 out_vtx[]) {
    __m128 p0 = in_vtx[in_vtx_count - 1];
    __m128 dist0 = _mm128_dp4_ps(plane, p0);

    SWint out_vtx_count = 0;
    for (SWint i = 0; i < in_vtx_count; i++) {
        const __m128 p1 = in_vtx[i];
        const __m128 dist1 = _mm128_dp4_ps(plane, p1);
        const int dist0_neg = _mm128_movemask_ps(dist0);
        if (!dist0_neg) { // dist0 >= 0.0f
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

void swCullCtxDebugDepth(SWcull_ctx *ctx, SWfloat *out_depth) { (*ctx->debug_depth_proc)(ctx, out_depth); }
