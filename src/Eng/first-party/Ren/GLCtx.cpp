#include "GLCtx.h"

#include "GL.h"

bool Ren::ReadbackTimestampQueries(ApiContext *api_ctx, int i) {
    const uint32_t query_count = api_ctx->query_counts[i];
    if (!query_count) {
        // nothing to readback
        return true;
    }

    for (uint32_t j = 0; j < query_count; ++j) {
        glGetQueryObjectui64v(api_ctx->queries[i][j], GL_QUERY_RESULT, &api_ctx->query_results[i][j]);
    }
    api_ctx->query_counts[i] = 0;

    return true;
}