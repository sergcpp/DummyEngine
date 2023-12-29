#ifndef SW_CULLING_H
#define SW_CULLING_H

#include "SWcore.h"
#include "SWcpu.h"
#include "SWzbuffer.h"

#define SW_CULL_TILE_WIDTH_SHIFT 5
#define SW_CULL_TILE_SIZE_X (1 << SW_CULL_TILE_WIDTH_SHIFT)

#define SW_CULL_SUBTILE_X 8

#define SW_CULL_QUICK_MASK

typedef enum SWsurf_type { SW_OCCLUDER = 0, SW_OCCLUDEE } SWsurf_type;

typedef struct SWcull_surf {
    SWsurf_type type;
    SWenum prim_type, index_type;
    const void *attribs;
    const void *indices;
    SWuint stride, count;
    SWfloat bbox_min[3], bbox_max[3];
    const SWfloat *xform;
    SWint visible;
} SWcull_surf;

/************************************************************************/

struct SWcull_ctx;
typedef SWint (*SWCullTrianglesIndexedProcType)(struct SWcull_ctx *ctx,
                                                const void *attribs,
                                                const SWuint *indices, SWuint stride,
                                                SWuint index_count, const SWfloat *xform,
                                                SWint is_occluder);
typedef SWint (*SWCullRectProcType)(const struct SWcull_ctx *ctx, const SWfloat p_min[2],
                                    const SWfloat p_max[3], const SWfloat w_min);
typedef void (*SWCullClearBufferProcType)(struct SWcull_ctx *ctx);
typedef void (*SWCullDebugDepthProcType)(const struct SWcull_ctx *ctx,
                                         SWfloat *out_depth);

enum eClipPlane { Left, Right, Top, Bottom, Near, _PlanesCount };

typedef struct SWcull_ctx {
    SWcpu_info cpu_info;

    SWint w, h;
    SWfloat half_w, half_h;
    SWfloat near_clip;

    SWint tile_size_y, subtile_size_y;
    SWint tile_w, tile_h;
    void *ztiles;
    SWuint ztiles_mem_size;

    SWCullTrianglesIndexedProcType tri_indexed_proc;
    SWCullRectProcType test_rect_proc;
    SWCullClearBufferProcType clear_buf_proc;
    SWCullDebugDepthProcType debug_depth_proc;

    ALIGNED(SWint size_ivec4[4], 16);
    ALIGNED(SWfloat half_size_vec4[4], 16);

    ALIGNED(SWfloat clip_planes[_PlanesCount][4], 16);
} SWcull_ctx;

void swCullCtxInit(SWcull_ctx *ctx, SWint w, SWint h, SWfloat near_clip);
void swCullCtxDestroy(SWcull_ctx *ctx);

void swCullCtxResize(SWcull_ctx *ctx, SWint w, SWint h, SWfloat near_clip);
void swCullCtxClear(SWcull_ctx *ctx);
void swCullCtxSubmitCullSurfs(SWcull_ctx *ctx, SWcull_surf *surfs, SWuint count);

SWint swCullCtxTestRect(SWcull_ctx *ctx, const SWfloat p_min[2], const SWfloat p_max[3],
                        SWfloat w_min);

void swCullCtxDebugDepth(SWcull_ctx *ctx, SWfloat *out_depth);

#endif /* SW_CULLING_H */