#include "FrameBuf.h"

#include <stdexcept>

#include <Ren/Fwd.h>
#include <Ren/GL.h>
#include <Sys/Log.h>

FrameBuf::FrameBuf(int _w, int _h, const ColorAttachmentDesc *_attachments, int attachments_count,
                   bool with_depth, Ren::eTexFilter depth_filter, int _msaa)
    : w(_w), h(_h), sample_count(_msaa) {

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, w, h);

    for (int i = 0; i < attachments_count; i++) {
        const auto &att = _attachments[i];

        GLuint _col_tex;

        glGenTextures(1, &_col_tex);
        glActiveTexture(GL_TEXTURE0);

        Ren::CheckError("[Renderer]: create framebuffer 1");

        GLenum format = Ren::GLFormatFromTexFormat(att.format),
               internal_format = Ren::GLInternalFormatFromTexFormat(att.format),
               type = Ren::GLTypeFromTexFormat(att.format);
        if (format == 0xffffffff || internal_format == 0xffffffff) {
            throw std::invalid_argument("Wrong format!");
        }

        if (sample_count > 1) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _col_tex);
            glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, sample_count, internal_format, w, h, GL_TRUE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D_MULTISAMPLE, _col_tex, 0);
        } else {
            glBindTexture(GL_TEXTURE_2D, _col_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, type, NULL);

            if (att.filter == Ren::NoFilter) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            } else if (att.filter == Ren::Bilinear) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

                glGenerateMipmap(GL_TEXTURE_2D);
            } else if (att.filter == Ren::BilinearNoMipmap) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }

            if (att.repeat == Ren::ClampToEdge) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            } else if (att.repeat == Ren::Repeat) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, _col_tex, 0);
        }

        attachments.push_back({ att, _col_tex });
    }

    if (attachments_count) {
        glClear(GL_COLOR_BUFFER_BIT);
    }
    Ren::CheckError("[Renderer]: create framebuffer 2");

    if (with_depth && attachments_count == 0) {
        GLuint _depth_tex;

        glGenTextures(1, &_depth_tex);
        glBindTexture(GL_TEXTURE_2D, _depth_tex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, w, h);

        // multisample textures does not support sampler state
        if (sample_count == 1) {
            if (depth_filter == Ren::NoFilter) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            } else if (depth_filter == Ren::Bilinear || depth_filter == Ren::BilinearNoMipmap) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depth_tex, 0);

        GLenum bufs[] = { GL_NONE };
        glDrawBuffers(1, bufs);

        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        LOGI("%ix%i", w, h);
        auto s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (s != GL_FRAMEBUFFER_COMPLETE) {
            LOGI("Frambuffer error %i", int(s));
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            throw std::runtime_error("Framebuffer error!");
        }

        depth_tex = _depth_tex;
        glClear(GL_DEPTH_BUFFER_BIT);
    } else if (with_depth) {
#if 0
        GLuint _depth_rb;

        glGenRenderbuffers(1, &_depth_rb);
        glBindRenderbuffer(GL_RENDERBUFFER, _depth_rb);
        if (sample_count > 1) {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, sample_count, GL_DEPTH_COMPONENT16, w, h);
        } else {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
        }
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depth_rb);
#else
        GLuint _depth_tex;

        GLenum target = sample_count > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

        glGenTextures(1, &_depth_tex);
        glBindTexture(target, _depth_tex);

        if (sample_count > 1) {
            glTexStorage2DMultisample(target, sample_count, GL_DEPTH_COMPONENT16, w, h, GL_TRUE);
        } else {
            glTexStorage2D(target, 1, GL_DEPTH_COMPONENT16, w, h);
        }

        Ren::CheckError("[Renderer]: create framebuffer 3");

        // multisample textures does not support sampler state
        if (sample_count == 1) {
            if (depth_filter == Ren::NoFilter) {
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            } else if (depth_filter == Ren::Bilinear || depth_filter == Ren::BilinearNoMipmap) {
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }

            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, _depth_tex, 0);

        depth_tex = _depth_tex;

        //GLenum bufs[] = { GL_NONE };
        //glDrawBuffers(1, bufs);

        //glReadBuffer(GL_NONE);
        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif

        LOGI("- %ix%i", w, h);
        auto s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (s != GL_FRAMEBUFFER_COMPLETE) {
            LOGI("Frambuffer error %i", int(s));
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            throw std::runtime_error("Framebuffer error!");
        }

        glClear(GL_DEPTH_BUFFER_BIT);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);

    Ren::CheckError("[Renderer]: create framebuffer 3");
    LOGI("Framebuffer created (%ix%i)", w, h);
}

FrameBuf::FrameBuf(FrameBuf &&rhs) {
    *this = std::move(rhs);
}

FrameBuf &FrameBuf::operator=(FrameBuf &&rhs) {
    attachments = std::move(rhs.attachments);
    w = rhs.w;
    h = rhs.h;
    sample_count = rhs.sample_count;
    fb = rhs.fb;
    depth_tex = std::move(rhs.depth_tex);
    depth_rb = std::move(rhs.depth_rb);

    rhs.w = rhs.h = -1;

    return *this;
}

FrameBuf::~FrameBuf() {
    for (const auto &att : attachments) {
        GLuint val = (GLuint)att.tex;
        glDeleteTextures(1, &val);
    }

    if (depth_tex.initialized()) {
        GLuint val = (GLuint)depth_tex.GetValue();
        glDeleteTextures(1, &val);
    }
    if (depth_rb.initialized()) {
        GLuint val = (GLuint)depth_rb.GetValue();
        glDeleteRenderbuffers(1, &val);
    }

    if (w != -1) {
        glDeleteFramebuffers(1, &fb);
    }
}
