#include "Framebuffer.h"

#include "GL.h"

Ren::Framebuffer::~Framebuffer() {
    if (id_) {
        auto fb = GLuint(id_);
        glDeleteFramebuffers(1, &fb);
    }
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h,
                             const WeakTex2DRef _depth_attachment, const WeakTex2DRef _stencil_attachment,
                             const WeakTex2DRef _color_attachments[], const int _color_attachments_count,

                             const bool is_multisampled) {
    if (((!_depth_attachment && !depth_attachment.ref) ||
         (_depth_attachment && _depth_attachment->handle() == depth_attachment.handle)) &&
        ((!_stencil_attachment && !stencil_attachment.ref) ||
         (_stencil_attachment && _stencil_attachment->handle() == stencil_attachment.handle)) &&
        _color_attachments_count == color_attachments.size() &&
        std::equal(_color_attachments, _color_attachments + _color_attachments_count, color_attachments.data(),
                   [](const WeakTex2DRef &lhs, const Attachment &rhs) {
                       return (!lhs && !rhs.ref) || (lhs && lhs->handle() == rhs.handle);
                   })) {
        // nothing has changed
        return true;
    }

    if (_color_attachments_count == 1 && !_color_attachments[0]) {
        // default backbuffer
        return true;
    }

    SmallVector<RenderTarget, MaxRTAttachments> color_targets;
    for (int i = 0; i < _color_attachments_count; ++i) {
        color_targets.emplace_back(_color_attachments[i], eLoadOp::DontCare, eStoreOp::DontCare);
    }

    return Setup(api_ctx, render_pass, w, h, RenderTarget(_depth_attachment, eLoadOp::DontCare, eStoreOp::DontCare),
                 RenderTarget(_stencil_attachment, eLoadOp::DontCare, eStoreOp::DontCare), color_targets.data(),
                 int(color_targets.size()));
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h,
                             const RenderTarget &_depth_target, const RenderTarget &_stencil_target,
                             const RenderTarget _color_targets[], int _color_targets_count) {
    if (((!_depth_target && !depth_attachment.ref) ||
         (_depth_target && _depth_target.ref->handle() == depth_attachment.handle)) &&
        ((!_stencil_target && !stencil_attachment.ref) ||
         (_stencil_target && _stencil_target.ref->handle() == stencil_attachment.handle)) &&
        _color_targets_count == color_attachments.size() &&
        std::equal(_color_targets, _color_targets + _color_targets_count, color_attachments.data(),
                   [](const RenderTarget &lhs, const Attachment &rhs) {
                       return (!lhs && !rhs.ref) || (lhs && lhs.ref->handle() == rhs.handle);
                   })) {
        // nothing has changed
        return true;
    }

    if (_color_targets_count == 1 && (!_color_targets[0] || _color_targets[0].ref->id() == 0)) {
        // default backbuffer
        return true;
    }

    if (!id_) {
        GLuint fb;
        glGenFramebuffers(1, &fb);
        id_ = uint32_t(fb);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(id_));

    GLenum target = GL_TEXTURE_2D;
    if ((!_color_targets_count || _color_targets[0].ref->params.samples > 1) &&
        (!_depth_target || _depth_target.ref->params.samples > 1) &&
        (!_stencil_target || _stencil_target.ref->params.samples > 1)) {
        target = GL_TEXTURE_2D_MULTISAMPLE;
    }

    for (size_t i = 0; i < color_attachments.size(); i++) {
        if (color_attachments[i].ref) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GLenum(i), target, 0, 0);
            color_attachments[i] = {};
        }
    }
    color_attachments.clear();

    SmallVector<GLenum, 4> draw_buffers;
    for (int i = 0; i < _color_targets_count; i++) {
        if (_color_targets[i]) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target,
                                   GLuint(_color_targets[i].ref->id()), 0);

            draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);

            color_attachments.push_back({_color_targets[i].ref, _color_targets[i].ref->handle()});
        } else {
            draw_buffers.push_back(GL_NONE);

            color_attachments.emplace_back();
        }
    }

    glDrawBuffers(GLsizei(color_attachments.size()), draw_buffers.data());

    if (_depth_target) {
        if (_depth_target == _stencil_target) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, target, GLuint(_depth_target.ref->id()),
                                   0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, GLuint(_depth_target.ref->id()), 0);
        }
        depth_attachment = {_depth_target.ref, _depth_target.ref->handle()};
    } else {
        depth_attachment = {};
    }

    if (_stencil_target) {
        if (!_depth_target || _depth_target.ref->handle() != _stencil_target.ref->handle()) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, target, GLuint(_stencil_target.ref->id()), 0);
        }
        stencil_attachment = {_stencil_target.ref, _stencil_target.ref->handle()};
    } else {
        stencil_attachment = {};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    return (s == GL_FRAMEBUFFER_COMPLETE);
}