#include "Framebuffer.h"

#include "GL.h"

#ifndef NDEBUG
#define VERBOSE_LOGGING
#endif

Ren::Framebuffer &Ren::Framebuffer::operator=(Framebuffer &&rhs) noexcept {
    if (&rhs == this) {
        return (*this);
    }

    Destroy();

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    id_ = std::exchange(rhs.id_, 0);
    w = std::exchange(rhs.w, -1);
    h = std::exchange(rhs.h, -1);
    color_attachments = std::move(rhs.color_attachments);
    depth_attachment = std::exchange(rhs.depth_attachment, {});
    stencil_attachment = std::exchange(rhs.stencil_attachment, {});

    return (*this);
}

Ren::Framebuffer::~Framebuffer() { Destroy(); }

void Ren::Framebuffer::Destroy() {
    if (id_) {
        auto fb = GLuint(id_);
        glDeleteFramebuffers(1, &fb);
    }
}

bool Ren::Framebuffer::Changed(const RenderPass &render_pass, const WeakImgRef &_depth_attachment,
                               const WeakImgRef &_stencil_attachment, Span<const WeakImgRef> _color_attachments) const {
    return depth_attachment != _depth_attachment || stencil_attachment != _stencil_attachment ||
           Span<const Attachment>(color_attachments) != _color_attachments;
}

bool Ren::Framebuffer::Changed(const RenderPass &render_pass, const WeakImgRef &_depth_attachment,
                               const WeakImgRef &_stencil_attachment,
                               Span<const RenderTarget> _color_attachments) const {
    return depth_attachment != _depth_attachment || stencil_attachment != _stencil_attachment ||
           Span<const Attachment>(color_attachments) != _color_attachments;
}

bool Ren::Framebuffer::LessThan(const RenderPass &render_pass, const WeakImgRef &_depth_attachment,
                                const WeakImgRef &_stencil_attachment,
                                Span<const WeakImgRef> _color_attachments) const {
    if (depth_attachment < _depth_attachment) {
        return true;
    } else if (depth_attachment == _depth_attachment) {
        if (stencil_attachment < _stencil_attachment) {
            return true;
        } else if (stencil_attachment == _stencil_attachment) {
            return Span<const Attachment>(color_attachments) < _color_attachments;
        }
    }
    return false;
}

bool Ren::Framebuffer::LessThan(const RenderPass &render_pass, const WeakImgRef &_depth_attachment,
                                const WeakImgRef &_stencil_attachment,
                                Span<const RenderTarget> _color_attachments) const {
    if (depth_attachment < _depth_attachment) {
        return true;
    } else if (depth_attachment == _depth_attachment) {
        if (stencil_attachment < _stencil_attachment) {
            return true;
        } else if (stencil_attachment == _stencil_attachment) {
            return Span<const Attachment>(color_attachments) < _color_attachments;
        }
    }
    return false;
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h,
                             const WeakImgRef _depth_attachment, const WeakImgRef _stencil_attachment,
                             Span<const WeakImgRef> _color_attachments, const bool is_multisampled, ILog *log) {
    if (!Changed(render_pass, _depth_attachment, _stencil_attachment, _color_attachments)) {
        // nothing has changed
        return true;
    }

    if (_color_attachments.size() == 1 && !_color_attachments[0]) {
        // default backbuffer
        return true;
    }

    SmallVector<RenderTarget, 4> color_targets;
    for (int i = 0; i < _color_attachments.size(); ++i) {
        color_targets.emplace_back(_color_attachments[i], eLoadOp::DontCare, eStoreOp::DontCare);
    }

    return Setup(
        api_ctx, render_pass, w, h, RenderTarget(std::move(_depth_attachment), eLoadOp::DontCare, eStoreOp::DontCare),
        RenderTarget(std::move(_stencil_attachment), eLoadOp::DontCare, eStoreOp::DontCare), color_targets, log);
}

bool Ren::Framebuffer::Setup(ApiContext *api_ctx, const RenderPass &render_pass, int w, int h,
                             const RenderTarget &_depth_target, const RenderTarget &_stencil_target,
                             Span<const RenderTarget> _color_targets, ILog *log) {
    SmallVector<WeakImgRef, 4> color_refs;
    for (int i = 0; i < _color_targets.size(); ++i) {
        color_refs.push_back(_color_targets[i].ref);
    }
    if (!Changed(render_pass, _depth_target.ref, _stencil_target.ref, color_refs)) {
        // nothing has changed
        return true;
    }

    if (_color_targets.size() == 1 && (!_color_targets[0] || _color_targets[0].ref->id() == 0)) {
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
    if ((_color_targets.empty() || _color_targets[0].ref->params.samples > 1) &&
        (!_depth_target || _depth_target.ref->params.samples > 1) &&
        (!_stencil_target || _stencil_target.ref->params.samples > 1)) {
        target = GL_TEXTURE_2D_MULTISAMPLE;
    }
    bool cube = false;
    if (!_color_targets.empty() && (Bitmask<eImgFlags>{_color_targets[0].ref->params.flags} & eImgFlags::CubeMap)) {
        cube = true;
        target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

    for (size_t i = 0; i < color_attachments.size(); i++) {
        if (color_attachments[i].ref) {
            if (cube) {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GLenum(i),
                                       target + color_attachments[i].view_index - 1, 0, 0);
            } else {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GLenum(i), target, 0, 0);
            }
            color_attachments[i] = {};
        }
    }
    color_attachments.clear();

    SmallVector<GLenum, 4> draw_buffers;
    for (int i = 0; i < _color_targets.size(); i++) {
        if (_color_targets[i]) {
            if (cube) {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + GLenum(i),
                                       target + _color_targets[i].view_index - 1, GLuint(_color_targets[i].ref->id()),
                                       0);
            } else {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target,
                                       GLuint(_color_targets[i].ref->id()), 0);
            }
            draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);

            color_attachments.push_back(
                {_color_targets[i].ref, _color_targets[i].view_index, _color_targets[i].ref->handle()});
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
        depth_attachment = {_depth_target.ref, _depth_target.view_index, _depth_target.ref->handle()};
    } else {
        depth_attachment = {};
    }

    if (_stencil_target) {
        if (!_depth_target || _depth_target.ref->handle() != _stencil_target.ref->handle()) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, target, GLuint(_stencil_target.ref->id()), 0);
        }
        stencil_attachment = {_stencil_target.ref, _stencil_target.view_index, _stencil_target.ref->handle()};
    } else {
        stencil_attachment = {};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (s != GL_FRAMEBUFFER_COMPLETE) {
        log->Error("Framebuffer creation failed (error %i)", int(s));
#ifdef VERBOSE_LOGGING
    } else {
        log->Info("Framebuffer %u created", id_);
#endif
    }
    return (s == GL_FRAMEBUFFER_COMPLETE);
}

#undef VERBOSE_LOGGING
