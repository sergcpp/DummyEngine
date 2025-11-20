#pragma once

#include <cstdint>

#include "Image.h"
#include "ImageParams.h"
#include "RenderPass.h"
#include "SmallVector.h"
#include "Span.h"

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
        WeakImgRef ref;
        uint8_t view_index = 0;
        ImgHandle handle; // handle is stored to detect image reallocation

        bool operator==(const WeakImgRef &rhs) const {
            if (!rhs) {
                return !bool(this->ref);
            }
            return this->handle == rhs->handle();
        }
        bool operator!=(const WeakImgRef &rhs) const { return !operator==(rhs); }
        bool operator<(const WeakImgRef &rhs) const {
            if (!rhs) {
                return this->handle < ImgHandle();
            }
            return this->handle < rhs->handle();
        }
        friend bool operator<(const WeakImgRef &lhs, const Attachment &rhs) {
            if (!lhs) {
                return ImgHandle() < rhs.handle;
            }
            return lhs->handle() < rhs.handle;
        }

        bool operator==(const RenderTarget &rhs) const {
            if (!rhs) {
                return !bool(this->ref);
            }
            return this->handle == rhs.ref->handle() && this->view_index == rhs.view_index;
        }
        bool operator!=(const RenderTarget &rhs) const { return !operator==(rhs); }
        bool operator<(const RenderTarget &rhs) const {
            if (!rhs) {
                return this->handle < ImgHandle();
            }
            if (this->handle < rhs.ref->handle()) {
                return true;
            } else if (this->handle == rhs.ref->handle()) {
                return this->view_index < rhs.view_index;
            }
            return false;
        }
        friend bool operator<(const RenderTarget &lhs, const Attachment &rhs) {
            if (!lhs) {
                return ImgHandle() < rhs.handle;
            }
            if (lhs.ref->handle() < rhs.handle) {
                return true;
            } else if (lhs.ref->handle() == rhs.handle) {
                return lhs.view_index < rhs.view_index;
            }
            return false;
        }
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
    [[nodiscard]] VkFramebuffer vk_handle() const { return handle_; }
    [[nodiscard]] VkRenderPass renderpass() const { return renderpass_; }
#elif defined(REN_GL_BACKEND)
    [[nodiscard]] uint32_t id() const { return id_; }
#endif

    [[nodiscard]] bool Changed(const RenderPass &render_pass, const WeakImgRef &depth_attachment,
                               const WeakImgRef &stencil_attachment, Span<const WeakImgRef> color_attachments) const;
    [[nodiscard]] bool Changed(const RenderPass &render_pass, const WeakImgRef &depth_attachment,
                               const WeakImgRef &stencil_attachment, Span<const RenderTarget> color_attachments) const;

    [[nodiscard]] bool LessThan(const RenderPass &render_pass, const WeakImgRef &depth_attachment,
                                const WeakImgRef &stencil_attachment, Span<const WeakImgRef> color_attachments) const;
    [[nodiscard]] bool LessThan(const RenderPass &render_pass, const WeakImgRef &depth_attachment,
                                const WeakImgRef &stencil_attachment, Span<const RenderTarget> color_attachments) const;

    bool Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h, WeakImgRef depth_attachment,
               WeakImgRef stencil_attachment, Span<const WeakImgRef> color_attachments, bool is_multisampled,
               ILog *log);
    bool Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h, const RenderTarget &depth_target,
               const RenderTarget &stencil_target, Span<const RenderTarget> color_attachments, ILog *log);
    bool Setup(ApiContext *api_ctx, const RenderPass &renderpass, int w, int h, const WeakImgRef depth_attachment,
               const WeakImgRef stencil_attachment, const WeakImgRef color_attachment, const bool is_multisampled,
               ILog *log) {
        return Setup(api_ctx, renderpass, w, h, depth_attachment, stencil_attachment, {&color_attachment, 1},
                     is_multisampled, log);
    }
};
} // namespace Ren