#include "GraphBuilder.h"

#include <Ren/GLCtx.h>

int RpBuilder::WriteTimestamp(const bool start) {
    /*VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(ctx_.current_cmd_buf());
    Ren::ApiContext *api_ctx = ctx_.api_ctx();

    vkCmdWriteTimestamp(cmd_buf, start ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        api_ctx->query_pools[api_ctx->backend_frame], api_ctx->query_counts[api_ctx->backend_frame]);

    const uint32_t query_index = api_ctx->query_counts[api_ctx->backend_frame]++;
    assert(api_ctx->query_counts[api_ctx->backend_frame] < Ren::MaxTimestampQueries);
    return int(query_index);*/

    return 0;
}
