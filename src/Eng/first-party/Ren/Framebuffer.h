#pragma once

#include <cstdint>

#include "RenderPass.h"
#include "SmallVector.h"
#include "Span.h"
#include "Texture.h"
#include "TextureParams.h"

namespace Ren {
class Framebuffer {
    ApiContext *api_ctx_ = nullptr;
#if defined(REN_VK_BACKEND)
    VkFramebuffer handle_ = {};
    VkRenderPass renderpass_ = {};
#elif defined(REN_GL_BACKEND)
    uint32_t id_ = 0;
#endif
    struct Attachment {
        WeakTex2DRef ref;
        uint8_t view_index = 0;
        TexHandle handle; // handle is stored to detect texture reallocation
    };

    void Destroy();

  public:
    int w = -1, h = -1;

    SmallVector<Attachment, 4> color_attachments;
    Attachment depth_attachment, stencil_attachment;

    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer &rhs) = delete;
    Framebuffer(Framebuffer &&rhs) noexcept { (*this) = std::move(rhs); }
    Framebuffer &operator=(const Framebuffer &rhs) = delete;
    Framebuffer &operator=(Framebuffer &&rhs) noexcept;

#if defined(REN_VK_BACKEND)
    [[nodiscard]] VkFramebuffer handle() const { return handle_; }
    [[nodiscard]] VkRenderPass renderpass() const { return renderpass_; }
#elif defined(REN_GL_BACKEND)
    uint32_t id() const { return id_; }
#endif

    [[nodiscard]] bool Changed(const RenderPass &render_pass, WeakTex2DRef _depth_attachment,
                               WeakTex2DRef _stencil_attachment, Span<const WeakTex2DRef> _color_attachments) const;
    [[nodiscard]] bool Changed(const RenderPass &render_pass, WeakTex2DRef _depth_attachment,
                               WeakTex2DRef _stencil_attachment, Span<const RenderTarget> _color_attachments) const;

    bool Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h, WeakTex2DRef depth_attachment,
               WeakTex2DRef stencil_attachment, Span<const WeakTex2DRef> color_attachments, bool is_multisampled,
               ILog *log);
    bool Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h, const RenderTarget &depth_target,
               const RenderTarget &stencil_target, Span<const RenderTarget> color_attachments, ILog *log);
    bool Setup(ApiContext *api_ctx, const RenderPass &renderpass, int w, int h, const WeakTex2DRef depth_attachment,
               const WeakTex2DRef stencil_attachment, const WeakTex2DRef color_attachment, const bool is_multisampled,
               ILog *log) {
        return Setup(api_ctx, renderpass, w, h, depth_attachment, stencil_attachment, {&color_attachment, 1},
                     is_multisampled, log);
    }
};
} // namespace Ren