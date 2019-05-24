#pragma once

#include <Ren/Texture.h>
#include <Sys/Optional.h>

struct FrameBuf {
    int w = -1, h = -1, sample_count = 0;

    struct ColorAttachmentDesc {
        Ren::eTexColorFormat format;
        Ren::eTexFilter filter;
        Ren::eTexRepeat repeat;
    };

#if defined(USE_GL_RENDER)
    struct ColorAttachment {
        ColorAttachmentDesc desc;
        uint32_t tex;
    };

    uint32_t fb;
    Sys::Optional<uint32_t> depth_tex;
    ColorAttachment attachments[4];
    uint32_t        attachments_count = 0;
#endif
    FrameBuf() :  w(-1), h(-1), fb(0xffffffff), attachments_count(0) {}
    FrameBuf(int w, int h, const ColorAttachmentDesc *attachments, int attachments_count,
             bool with_depth = true, Ren::eTexFilter depth_filter = Ren::NoFilter, int sample_count = 1);
    ~FrameBuf();

    FrameBuf(const FrameBuf &rhs) = delete;
    FrameBuf &operator=(const FrameBuf &rhs) = delete;
    FrameBuf(FrameBuf &&rhs);
    FrameBuf &operator=(FrameBuf &&rhs);
};
