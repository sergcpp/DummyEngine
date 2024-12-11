#include "Framebuffer.h"

#include "VKCtx.h"

#ifndef NDEBUG
#define VERBOSE_LOGGING
#endif

Ren::Framebuffer &Ren::Framebuffer::operator=(Framebuffer &&rhs) noexcept {
    if (&rhs == this) {
        return (*this);
    }

    Destroy();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, VkFramebuffer{VK_NULL_HANDLE});
    renderpass_ = std::exchange(rhs.renderpass_, VkRenderPass{VK_NULL_HANDLE});
    w = std::exchange(rhs.w, -1);
    h = std::exchange(rhs.h, -1);
    color_attachments = std::move(rhs.color_attachments);
    depth_attachment = std::exchange(rhs.depth_attachment, {});
    stencil_attachment = std::exchange(rhs.stencil_attachment, {});

    return (*this);
}

Ren::Framebuffer::~Framebuffer() { Destroy(); }

void Ren::Framebuffer::Destroy() {
    if (handle_ != VK_NULL_HANDLE) {
        api_ctx_->framebuffers_to_destroy[api_ctx_->backend_frame].push_back(handle_);
        handle_ = VK_NULL_HANDLE;
    }
}

bool Ren::Framebuffer::Changed(const RenderPass &render_pass, const WeakTex2DRef &_depth_attachment,
                               const WeakTex2DRef &_stencil_attachment,
                               Span<const WeakTex2DRef> _color_attachments) const {
    return renderpass_ != render_pass.handle() || depth_attachment != _depth_attachment ||
           stencil_attachment != _stencil_attachment || Span<const Attachment>(color_attachments) != _color_attachments;
}

bool Ren::Framebuffer::Changed(const RenderPass &render_pass, const WeakTex2DRef &_depth_attachment,
                               const WeakTex2DRef &_stencil_attachment,
                               Span<const RenderTarget> _color_attachments) const {
    return renderpass_ != render_pass.handle() || depth_attachment != _depth_attachment ||
           stencil_attachment != _stencil_attachment || Span<const Attachment>(color_attachments) != _color_attachments;
}

bool Ren::Framebuffer::LessThan(const RenderPass &render_pass, const WeakTex2DRef &_depth_attachment,
                                const WeakTex2DRef &_stencil_attachment,
                                Span<const WeakTex2DRef> _color_attachments) const {
    if (renderpass_ < render_pass.handle()) {
        return true;
    } else if (renderpass_ == render_pass.handle()) {
        if (depth_attachment < _depth_attachment) {
            return true;
        } else if (depth_attachment == _depth_attachment) {
            if (stencil_attachment < _stencil_attachment) {
                return true;
            } else if (stencil_attachment == _stencil_attachment) {
                return Span<const Attachment>(color_attachments) < _color_attachments;
            }
        }
    }
    return false;
}

bool Ren::Framebuffer::LessThan(const RenderPass &render_pass, const WeakTex2DRef &_depth_attachment,
                                const WeakTex2DRef &_stencil_attachment,
                                Span<const RenderTarget> _color_attachments) const {
    if (renderpass_ < render_pass.handle()) {
        return true;
    } else if (renderpass_ == render_pass.handle()) {
        if (depth_attachment < _depth_attachment) {
            return true;
        } else if (depth_attachment == _depth_attachment) {
            if (stencil_attachment < _stencil_attachment) {
                return true;
            } else if (stencil_attachment == _stencil_attachment) {
                return Span<const Attachment>(color_attachments) < _color_attachments;
            }
        }
    }
    return false;
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, const RenderPass &render_pass, const int _w, const int _h,
                             const WeakTex2DRef _depth_attachment, const WeakTex2DRef _stencil_attachment,
                             Span<const WeakTex2DRef> _color_attachments, const bool is_multisampled, ILog *log) {
    if (!Changed(render_pass, _depth_attachment, _stencil_attachment, _color_attachments)) {
        // nothing has changed
        return true;
    }

    Destroy();

    api_ctx_ = api_ctx;
    color_attachments.clear();
    depth_attachment = {};
    stencil_attachment = {};

    SmallVector<VkImageView, 4> image_views;

    if (_depth_attachment) {
        image_views.push_back(_depth_attachment->handle().views[0]);
        depth_attachment = {_depth_attachment, 0, _depth_attachment->handle()};
    }

    if (_stencil_attachment) {
        stencil_attachment = {_stencil_attachment, 0, _stencil_attachment->handle()};
        if (_stencil_attachment->handle().views[0] != _depth_attachment->handle().views[0]) {
            image_views.push_back(_stencil_attachment->handle().views[0]);
        }
    }

    for (int i = 0; i < _color_attachments.size(); i++) {
        if (_color_attachments[i]) {
            image_views.push_back(_color_attachments[i]->handle().views[0]);
            color_attachments.push_back({_color_attachments[i], 0, _color_attachments[i]->handle()});
        } else {
            color_attachments.emplace_back();
        }
    }

    renderpass_ = render_pass.handle();
    w = _w;
    h = _h;

    VkFramebufferCreateInfo framebuf_create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuf_create_info.renderPass = renderpass_;
    framebuf_create_info.attachmentCount = uint32_t(image_views.size());
    framebuf_create_info.pAttachments = image_views.data();
    framebuf_create_info.width = _w;
    framebuf_create_info.height = _h;
    framebuf_create_info.layers = 1;

    const VkResult res = api_ctx->vkCreateFramebuffer(api_ctx->device, &framebuf_create_info, nullptr, &handle_);
    if (res != VK_SUCCESS) {
        log->Error("Framebuffer creation failed (error %i)", int(res));
#ifdef VERBOSE_LOGGING
    } else {
        log->Info("Framebuffer %p created (%i attachments)", handle_, int(image_views.size()));
#endif
    }
    return res == VK_SUCCESS;
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, const RenderPass &render_pass, const int _w, const int _h,
                             const RenderTarget &_depth_target, const RenderTarget &_stencil_target,
                             Span<const RenderTarget> _color_targets, ILog *log) {
    SmallVector<WeakTex2DRef, 4> color_refs;
    for (int i = 0; i < _color_targets.size(); ++i) {
        color_refs.push_back(_color_targets[i].ref);
    }
    if (!Changed(render_pass, _depth_target.ref, _stencil_target.ref, color_refs)) {
        // nothing has changed
        return true;
    }

    /*if (_color_attachments_count == 1 && !_color_attachments[0]) {
        // default backbuffer
        return true;
    }*/

    Destroy();

    api_ctx_ = api_ctx;
    color_attachments.clear();
    depth_attachment = {};
    stencil_attachment = {};

    SmallVector<VkImageView, 4> image_views;

    if (_depth_target) {
        image_views.push_back(_depth_target.ref->handle().views[0]);
        depth_attachment = {_depth_target.ref, _depth_target.view_index, _depth_target.ref->handle()};
    }

    if (_stencil_target) {
        stencil_attachment = {_stencil_target.ref, _stencil_target.view_index, _stencil_target.ref->handle()};
        if (!_depth_target || _stencil_target.ref->handle().views[0] != _depth_target.ref->handle().views[0]) {
            image_views.push_back(_stencil_target.ref->handle().views[0]);
        }
    }

    for (int i = 0; i < _color_targets.size(); i++) {
        if (_color_targets[i]) {
            image_views.push_back(_color_targets[i].ref->handle().views[_color_targets[i].view_index]);
            color_attachments.push_back(
                {_color_targets[i].ref, _color_targets[i].view_index, _color_targets[i].ref->handle()});
        } else {
            color_attachments.emplace_back();
        }
    }

    renderpass_ = render_pass.handle();
    w = _w;
    h = _h;

    VkFramebufferCreateInfo framebuf_create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuf_create_info.renderPass = renderpass_;
    framebuf_create_info.attachmentCount = uint32_t(image_views.size());
    framebuf_create_info.pAttachments = image_views.data();
    framebuf_create_info.width = _w;
    framebuf_create_info.height = _h;
    framebuf_create_info.layers = 1;

    const VkResult res = api_ctx->vkCreateFramebuffer(api_ctx->device, &framebuf_create_info, nullptr, &handle_);
    if (res != VK_SUCCESS) {
        log->Error("Framebuffer creation failed (error %i)", int(res));
#ifdef VERBOSE_LOGGING
    } else {
        log->Info("Framebuffer %p created (%i attachments)", handle_, int(image_views.size()));
#endif
    }

    return res == VK_SUCCESS;
}

#undef VERBOSE_LOGGING
