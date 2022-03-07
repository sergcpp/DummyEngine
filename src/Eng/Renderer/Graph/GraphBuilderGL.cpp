#include "GraphBuilder.h"

#include <Ren/GLCtx.h>

int RpBuilder::WriteTimestamp(const bool) {
    Ren::ApiContext *api_ctx = ctx_.api_ctx();

    const uint32_t query_index = api_ctx->query_counts[api_ctx->backend_frame]++;
    glQueryCounter(api_ctx->queries[api_ctx->backend_frame][query_index], GL_TIMESTAMP);

    return int(query_index);
}
