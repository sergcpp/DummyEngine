#include "FrameBuf.h"

#include <stdexcept>

#include <Ren/Fwd.h>
#include <Ren/GL.h>

FrameBuf::FrameBuf(const char *name, Ren::Context &ctx, const int _w, const int _h,
                   const ColorAttachmentDesc *_attachments, int _attachments_count,
                   const DepthAttachmentDesc &depth_att, const int _msaa, Ren::ILog *log)
    : w(_w), h(_h), sample_count(_msaa) {

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, w, h);

    glActiveTexture(GL_TEXTURE0);

    int enabled_attachements_count = 0;

    for (int i = 0; i < _attachments_count; i++) {
        const ColorAttachmentDesc &att = _attachments[i];

        Ren::CheckError("[Renderer]: create framebuffer 1", log);

        char name_buf[32];
        sprintf(name_buf, "%s | col #%i", name, i);

        Ren::Tex2DParams params;
        params.w = w;
        params.h = h;
        params.format = att.format;
        params.filter = att.filter;
        params.repeat = att.repeat;
        params.samples = sample_count;

        Ren::eTexLoadStatus status;
        Ren::Tex2DRef tex = ctx.LoadTexture2D(name_buf, params, &status);

        if (att.attached) {
            ++enabled_attachements_count;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                   sample_count > 1 ? GL_TEXTURE_2D_MULTISAMPLE
                                                    : GL_TEXTURE_2D,
                                   tex->id(), 0);
        }

        attachments[attachments_count++] = {att, std::move(tex)};
    }

    if (enabled_attachements_count) {
        GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                         GL_COLOR_ATTACHMENT2};
        glDrawBuffers(enabled_attachements_count, bufs);

        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        GLenum bufs[] = {GL_NONE};
        glDrawBuffers(1, bufs);
    }
    Ren::CheckError("[Renderer]: create framebuffer 2", log);

    if (depth_att.format != Ren::eTexFormat::None) {
        char name_buf[32];
        sprintf(name_buf, "%s | depth", name);

        Ren::Tex2DParams params;
        params.w = w;
        params.h = h;
        params.format = depth_att.format;
        params.filter = depth_att.filter;
        params.samples = sample_count;

        Ren::eTexLoadStatus status;
        Ren::Tex2DRef dtex = ctx.LoadTexture2D(name_buf, params, &status);

        Ren::CheckError("[Renderer]: create framebuffer 3", log);

        if (depth_att.format == Ren::eTexFormat::Depth24Stencil8) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   sample_count > 1 ? GL_TEXTURE_2D_MULTISAMPLE
                                                    : GL_TEXTURE_2D,
                                   dtex->id(), 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   sample_count > 1 ? GL_TEXTURE_2D_MULTISAMPLE
                                                    : GL_TEXTURE_2D,
                                   dtex->id(), 0);
        }

        depth_tex = std::move(dtex);

        log->Info("- %ix%i", w, h);
        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (s != GL_FRAMEBUFFER_COMPLETE) {
            log->Error("Frambuffer error %i", int(s));
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            throw std::runtime_error("Framebuffer error!");
        }

        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_FRAMEBUFFER, fb, -1, name);
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2],
               viewport_before[3]);

    Ren::CheckError("[Renderer]: create framebuffer 3", log);
    log->Info("Framebuffer created (%ix%i)", w, h);
}

FrameBuf::FrameBuf(FrameBuf &&rhs) noexcept { *this = std::move(rhs); }

FrameBuf &FrameBuf::operator=(FrameBuf &&rhs) noexcept {
    for (uint32_t i = 0; i < rhs.attachments_count; i++) {
        attachments[i] = std::move(rhs.attachments[i]);
        rhs.attachments[i].desc.format = Ren::eTexFormat::Undefined;
    }
    attachments_count = rhs.attachments_count;
    w = rhs.w;
    h = rhs.h;
    sample_count = rhs.sample_count;
    fb = rhs.fb;
    depth_tex = std::move(rhs.depth_tex);

    rhs.w = rhs.h = -1;
    rhs.attachments_count = 0;

    return *this;
}

FrameBuf::~FrameBuf() {
    if (w != -1) {
        glDeleteFramebuffers(1, &fb);
    }
}
