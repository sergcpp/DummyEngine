#include "RpSSRPrepare.h"

#include <Ren/Buffer.h>
#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/VKCtx.h>

#include "../Renderer_Structs.h"

void RpSSRPrepare::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(ray_counter_buf_);
    RpAllocTex &raylen_tex = builder.GetWriteTexture(raylen_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();
    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    vkCmdFillBuffer(cmd_buf, ray_counter_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE, 0);

    VkClearColorValue clear_val = {};

    VkImageSubresourceRange clear_range = {};
    clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.layerCount = 1;
    clear_range.levelCount = 1;

    vkCmdClearColorImage(cmd_buf, raylen_tex.ref->handle().img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val, 1,
                         &clear_range);
}
