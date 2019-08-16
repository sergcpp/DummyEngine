#pragma once

#include <Ren/Texture.h>
#include <Sys/Optional.h>

struct FrameBuf {
    int w = -1, h = -1, sample_count = 0;

    struct ColorAttachmentDesc {
        Ren::eTexFormat format = Ren::eTexFormat::Undefined;
        Ren::eTexFilter filter = Ren::eTexFilter::NoFilter;
        Ren::eTexWrap wrap = Ren::eTexWrap::Repeat;
        bool attached = true;
    };

    struct DepthAttachmentDesc {
        Ren::eTexFormat format = Ren::eTexFormat::None;
        Ren::eTexFilter filter = Ren::eTexFilter::NoFilter;

        DepthAttachmentDesc() = default;
        DepthAttachmentDesc(Ren::eTexFormat _format, Ren::eTexFilter _filter) : format(_format), filter(_filter) {}
    };

#if defined(USE_GL_RENDER)
    struct ColorAttachment { // NOLINT
        ColorAttachmentDesc desc;
        Ren::Tex2DRef tex;
    };

    struct DepthAttachment {
        uint32_t tex;
        Ren::Tex2DRef tex2;
    };

    uint32_t fb;
    Ren::Tex2DRef depth_tex;
    ColorAttachment attachments[4];
    uint32_t attachments_count = 0;

    FrameBuf() : w(-1), h(-1), fb(0xffffffff), attachments_count(0) {} // NOLINT
#else
    FrameBuf() : w(-1), h(-1) {} // NOLINT
#endif
    FrameBuf(const char *name, Ren::Context &ctx, int w, int h, const ColorAttachmentDesc *attachments,
             int attachments_count, const DepthAttachmentDesc &depth_att, int sample_count, Ren::ILog *log);
    ~FrameBuf();

    FrameBuf(const FrameBuf &rhs) = delete;
    FrameBuf &operator=(const FrameBuf &rhs) = delete;
    FrameBuf(FrameBuf &&rhs) noexcept;
    FrameBuf &operator=(FrameBuf &&rhs) noexcept;
};
