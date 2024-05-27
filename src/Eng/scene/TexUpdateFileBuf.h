#pragma once

#include <Ren/Texture.h>
#include <Sys/AsyncFileReader.h>

#if defined(USE_VK_RENDER)
#include <Ren/VKCtx.h>
#endif

namespace Eng {
class TextureUpdateFileBuf : public Sys::FileReadBufBase {
    Ren::ApiContext *api_ctx_ = nullptr;
    Ren::Buffer stage_buf_;

  public:
    TextureUpdateFileBuf(Ren::ApiContext *api_ctx)
        : api_ctx_(api_ctx), stage_buf_("Tex Upload Buf", api_ctx, Ren::eBufType::Upload, 768, 768) {
        Realloc(24 * 1024 * 1024);

#if defined(USE_VK_RENDER)
        VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence new_fence;
        VkResult res = api_ctx->vkCreateFence(api_ctx->device, &fence_info, nullptr, &new_fence);
        assert(res == VK_SUCCESS);

        fence = Ren::SyncFence{api_ctx, new_fence};

        VkCommandBufferAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = api_ctx->command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer command_buf = {};
        res = api_ctx->vkAllocateCommandBuffers(api_ctx->device, &alloc_info, &command_buf);
        assert(res == VK_SUCCESS);

        cmd_buf = command_buf;
#endif
    }
    ~TextureUpdateFileBuf() override {
        Free();

#if defined(USE_VK_RENDER)
        VkCommandBuffer _cmd_buf = reinterpret_cast<VkCommandBuffer>(cmd_buf);
        api_ctx_->vkFreeCommandBuffers(api_ctx_->device, api_ctx_->command_pool, 1, &_cmd_buf);
#endif
    }

    Ren::Buffer &stage_buf() { return stage_buf_; }

    uint8_t *Alloc(const size_t new_size) override {
        stage_buf_.Resize(uint32_t(new_size));
        return stage_buf_.Map(true /* persistent */);
    }

    void Free() override {
        mem_ = nullptr;
        if (stage_buf_.is_mapped()) {
            stage_buf_.Unmap();
        }
        stage_buf_.Free();
    }

    Ren::SyncFence fence;
    Ren::CommandBuffer cmd_buf = {};
};
} // namespace Eng