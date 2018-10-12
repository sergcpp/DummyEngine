#include "FrameBuf.h"

#include <stdexcept>

#include <Ren/Fwd.h>
#include <Ren/GL.h>
#include <Sys/Log.h>

FrameBuf::FrameBuf(int _w, int _h, Ren::eTexColorFormat col_format, Ren::eTexFilter filter,
                   Ren::eTexRepeat repeat, bool with_depth, int _msaa)
    : col_format(col_format), w(_w), h(_h), msaa(_msaa) {
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    if (col_format != Ren::None) {
        GLuint _col_tex;

        glGenTextures(1, &_col_tex);
        glActiveTexture(GL_TEXTURE0);

        if (msaa > 1) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, _col_tex);

            if (col_format == Ren::RawRGB888) {
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGB8, w, h, GL_TRUE);
            } else if (col_format == Ren::RawRGBA8888) {
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGBA8, w, h, GL_TRUE);
            } else if (col_format == Ren::RawR32F) {
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_R32F, w, h, GL_TRUE);
            } else if (col_format == Ren::RawRGB16F) {
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGB16F, w, h, GL_TRUE);
            } else if (col_format == Ren::RawRGBA16F) {
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGBA16F, w, h, GL_TRUE);
            } else if (col_format == Ren::RawRGB32F) {
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGB32F, w, h, GL_TRUE);
            } else if (col_format == Ren::RawRGBA32F) {
                glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa, GL_RGBA32F, w, h, GL_TRUE);
            } else {
                throw std::invalid_argument("Wrong format!");
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, _col_tex, 0);
        } else {
            glBindTexture(GL_TEXTURE_2D, _col_tex);

            if (col_format == Ren::RawRGB888) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            } else if (col_format == Ren::RawRGBA8888) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            } else if (col_format == Ren::RawR32F) {
#if defined(EMSCRIPTEN)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
#else
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, NULL);
#endif
            } else if (col_format == Ren::RawRGB16F) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_HALF_FLOAT, NULL);
            } else if (col_format == Ren::RawRGBA16F) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
            } else if (col_format == Ren::RawRGB32F) {
#if defined(EMSCRIPTEN)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
#else
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, NULL);
#endif
            } else {
                throw std::invalid_argument("Wrong format!");
            }

            if (filter == Ren::NoFilter) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            } else if (filter == Ren::Bilinear || filter == Ren::BilinearNoMipmap) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }

            if (repeat == Ren::ClampToEdge) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            } else if (repeat == Ren::Repeat) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _col_tex, 0);
        }

        col_tex = _col_tex;

        glClear(GL_COLOR_BUFFER_BIT);

        Ren::CheckError("[Renderer]: create framebuffer 2");
    }

    if (with_depth && col_format == Ren::None) {
        GLuint _depth_tex;

        glGenTextures(1, &_depth_tex);
        glBindTexture(GL_TEXTURE_2D, _depth_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        if (filter == Ren::NoFilter) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        } else if (filter == Ren::Bilinear || filter == Ren::BilinearNoMipmap) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }

        if (repeat == Ren::ClampToEdge) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else if (repeat == Ren::Repeat) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depth_tex, 0);
//#if !defined(__ANDROID__)
        //glDrawBuffer(GL_NONE);
        GLenum bufs[] = { GL_NONE };
        glDrawBuffers(1, bufs);
//#endif
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
        GLuint _depth_rb;

        glGenRenderbuffers(1, &_depth_rb);
        glBindRenderbuffer(GL_RENDERBUFFER, _depth_rb);
        if (msaa > 1) {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_DEPTH_COMPONENT16, w, h);
        } else {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
        }
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depth_rb);

        LOGI("- %ix%i", w, h);
        auto s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (s != GL_FRAMEBUFFER_COMPLETE) {
            LOGI("Frambuffer error %i", int(s));
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            throw std::runtime_error("Framebuffer error!");
        }

        depth_rb = _depth_rb;

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Ren::CheckError("[Renderer]: create framebuffer 3");
    LOGI("Framebuffer created (%ix%i)", w, h);
}

FrameBuf::FrameBuf(FrameBuf &&rhs) {
    *this = std::move(rhs);
}

FrameBuf &FrameBuf::operator=(FrameBuf &&rhs) {
    col_format = rhs.col_format;
    w = rhs.w;
    h = rhs.h;
    msaa = rhs.msaa;
    fb = rhs.fb;
    col_tex = std::move(rhs.col_tex);
    depth_tex = std::move(rhs.depth_tex);
    depth_rb = std::move(rhs.depth_rb);

    rhs.col_format = Ren::Undefined;
    rhs.w = rhs.h = -1;

    return *this;
}

FrameBuf::~FrameBuf() {
    if (col_format != Ren::Undefined) {
        glDeleteFramebuffers(1, &fb);
        if (col_tex.initialized()) {
            GLuint val = (GLuint)col_tex.GetValue();
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

    }
}
