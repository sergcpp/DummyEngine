#ifndef SW_CULLING_H
#define SW_CULLING_H

#include "SWcore.h"
#include "SWzbuffer.h"

typedef enum SWsurf_type {
    SW_OCCLUDER = 0,
    SW_OCCLUDEE
} SWsurf_type;

typedef struct SWcull_surf {
    SWsurf_type type;
    SWenum prim_type, index_type;
    const void *attribs;
    const void *indices;
    SWuint stride, count;
    SWint base_vertex;
    SWfloat bbox_min[3], bbox_max[3];
    const SWfloat *xform;
    SWint visible, *dont_skip;
} SWcull_surf;

/************************************************************************/

typedef struct SWcull_ctx {
    SWzbuffer zbuf;
    float depth_eps;
} SWcull_ctx;

void swCullCtxInit(SWcull_ctx *ctx, SWint w, SWint h, SWfloat zmax);
void swCullCtxDestroy(SWcull_ctx *ctx);

void swCullCtxClear(SWcull_ctx *ctx);
void swCullCtxSubmitCullSurfs(SWcull_ctx *ctx, SWcull_surf *surfs, SWuint count);

SWint swCullCtxCullTrianglesIndexed_Ubyte(SWcull_ctx *ctx, const void *attribs, const SWubyte *indices,
        SWuint stride, SWuint count, SWint base_vertex, const SWfloat *xform, SWint is_occluder);

SWint swCullCtxCullTrianglesIndexed_Uint(SWcull_ctx *ctx, const void *attribs, const SWuint *indices,
        SWuint stride, SWuint count, SWint base_vertex, const SWfloat *xform, SWint is_occluder);

#endif /* SW_CULLING_H */