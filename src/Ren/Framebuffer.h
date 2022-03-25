#pragma once

#include <cstdint>

#include "RenderPass.h"
#include "SmallVector.h"
#include "Texture.h"
#include "TextureParams.h"

#if defined(USE_VK_RENDER)
#include "VK.h"
#endif

namespace Ren {
class Framebuffer {
    ApiContext *api_ctx_ = nullptr;
#if defined(USE_VK_RENDER)
    VkFramebuffer handle_ = VK_NULL_HANDLE;
    VkRenderPass renderpass_ = VK_NULL_HANDLE;
#elif defined(USE_GL_RENDER)
    uint32_t id_ = 0;
#endif
    struct Attachment {
        WeakTex2DRef ref;
        TexHandle handle; // handle is stored to detect texture reallocation
    };

    void Destroy();

  public:
    int w = -1, h = -1;

    SmallVector<Attachment, MaxRTAttachments> color_attachments;
    Attachment depth_attachment, stencil_attachment;

    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer &rhs) = delete;
    Framebuffer(Framebuffer &&rhs) noexcept { (*this) = std::move(rhs); }
    Framebuffer &operator=(const Framebuffer &rhs) = delete;
    Framebuffer &operator=(Framebuffer &&rhs) noexcept;

#if defined(USE_VK_RENDER)
    VkFramebuffer handle() const { return handle_; }
    VkRenderPass renderpass() const { return renderpass_; }
#elif defined(USE_GL_RENDER)
    uint32_t id() const { return id_; }
#endif

    bool Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h, WeakTex2DRef depth_attachment,
               WeakTex2DRef stencil_attachment, const WeakTex2DRef color_attachments[], int color_attachments_count,
               bool is_multisampled);
    bool Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h, const RenderTarget &depth_target,
               const RenderTarget &stencil_target, const RenderTarget color_attachments[], int color_attachments_count);
    bool Setup(ApiContext *api_ctx, const RenderPass &renderpass, int w, int h, const WeakTex2DRef depth_attachment,
               const WeakTex2DRef stencil_attachment, const WeakTex2DRef color_attachment, const bool is_multisampled) {
        return Setup(api_ctx, renderpass, w, h, depth_attachment, stencil_attachment, &color_attachment, 1,
                     is_multisampled);
    }
};
} // namespace Ren