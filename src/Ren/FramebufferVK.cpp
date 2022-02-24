#include "Framebuffer.h"

#include "VKCtx.h"

Ren::Framebuffer &Ren::Framebuffer::operator=(Framebuffer &&rhs) noexcept {
    if (&rhs == this) {
        return (*this);
    }

    Destroy();

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, VkFramebuffer{VK_NULL_HANDLE});
    renderpass_ = exchange(rhs.renderpass_, VkRenderPass{VK_NULL_HANDLE});
    w = exchange(rhs.w, -1);
    h = exchange(rhs.h, -1);
    color_attachments = std::move(rhs.color_attachments);
    depth_attachment = exchange(rhs.depth_attachment, {});
    stencil_attachment = exchange(rhs.stencil_attachment, {});

    return (*this);
}

Ren::Framebuffer::~Framebuffer() { Destroy(); }

void Ren::Framebuffer::Destroy() {
    if (handle_ != VK_NULL_HANDLE) {
        api_ctx_->framebuffers_to_destroy[api_ctx_->backend_frame].push_back(handle_);
        handle_ = VK_NULL_HANDLE;
    }
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, const RenderPass &render_pass, int _w, int _h,
                             const WeakTex2DRef _color_attachments[], const int _color_attachments_count,
                             const WeakTex2DRef _depth_attachment, const WeakTex2DRef _stencil_attachment,
                             const bool is_multisampled) {
    if (renderpass_ == render_pass.handle() && _color_attachments_count == color_attachments.size() &&
        std::equal(_color_attachments, _color_attachments + _color_attachments_count, color_attachments.data(),
                   [](const WeakTex2DRef &lhs, const Attachment &rhs) {
                       return (!lhs && !rhs.ref) || (lhs && lhs->handle() == rhs.handle);
                   }) &&
        ((!_depth_attachment && !depth_attachment.ref) ||
         (_depth_attachment && _depth_attachment->handle() == depth_attachment.handle)) &&
        ((!_stencil_attachment && !stencil_attachment.ref) ||
         (_stencil_attachment && _stencil_attachment->handle() == stencil_attachment.handle))) {
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
    for (int i = 0; i < _color_attachments_count; i++) {
        if (_color_attachments[i]) {
            image_views.push_back(_color_attachments[i]->handle().views[0]);
            color_attachments.push_back({_color_attachments[i], _color_attachments[i]->handle()});
        } else {
            color_attachments.emplace_back();
        }
    }

    if (_depth_attachment) {
        image_views.push_back(_depth_attachment->handle().views[0]);
        depth_attachment = {_depth_attachment, _depth_attachment->handle()};
    }

    if (_stencil_attachment) {
        stencil_attachment = {_stencil_attachment, _stencil_attachment->handle()};
        if (_stencil_attachment->handle().views[0] != _depth_attachment->handle().views[0]) {
            image_views.push_back(_stencil_attachment->handle().views[0]);
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

    const VkResult res = vkCreateFramebuffer(api_ctx->device, &framebuf_create_info, nullptr, &handle_);
    return res == VK_SUCCESS;
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, const RenderPass &render_pass, int _w, int _h,
                             const RenderTarget _color_targets[], int _color_targets_count,
                             const RenderTarget &_depth_target, const RenderTarget &_stencil_target) {
    if (renderpass_ == render_pass.handle() && _color_targets_count == color_attachments.size() &&
        std::equal(_color_targets, _color_targets + _color_targets_count, color_attachments.data(),
                   [](const RenderTarget &lhs, const Attachment &rhs) {
                       return (!lhs && !rhs.ref) || (lhs && lhs.ref->handle() == rhs.handle);
                   }) &&
        ((!_depth_target && !depth_attachment.ref) ||
         (_depth_target && _depth_target.ref->handle() == depth_attachment.handle)) &&
        ((!_stencil_target && !stencil_attachment.ref) ||
         (_stencil_target && _stencil_target.ref->handle() == stencil_attachment.handle))) {
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
    for (int i = 0; i < _color_targets_count; i++) {
        if (_color_targets[i]) {
            image_views.push_back(_color_targets[i].ref->handle().views[0]);
            color_attachments.push_back({_color_targets[i].ref, _color_targets[i].ref->handle()});
        } else {
            color_attachments.emplace_back();
        }
    }

    if (_depth_target) {
        image_views.push_back(_depth_target.ref->handle().views[0]);
        depth_attachment = {_depth_target.ref, _depth_target.ref->handle()};
    }

    if (_stencil_target) {
        stencil_attachment = {_stencil_target.ref, _stencil_target.ref->handle()};
        if (!_depth_target || _stencil_target.ref->handle().views[0] != _depth_target.ref->handle().views[0]) {
            image_views.push_back(_stencil_target.ref->handle().views[0]);
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

    const VkResult res = vkCreateFramebuffer(api_ctx->device, &framebuf_create_info, nullptr, &handle_);
    return res == VK_SUCCESS;
}