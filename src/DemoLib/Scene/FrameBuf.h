#pragma once

#include <Ren/Texture.h>
#include <Sys/Optional.h>

struct FrameBuf {
    Ren::eTex2DFormat format;
    int w = -1, h = -1;

#if defined(USE_GL_RENDER)
    uint32_t fb, col_tex;
    Sys::Optional<uint32_t> depth_rb;
#endif
    FrameBuf() : format(Ren::Undefined), w(-1), h(-1) {}
    FrameBuf(int w, int h, Ren::eTex2DFormat format, Ren::eTexFilter filter,
                Ren::eTexRepeat repeat, bool with_depth = true);
    ~FrameBuf();

    FrameBuf(const FrameBuf &rhs) = delete;
    FrameBuf &operator=(const FrameBuf &rhs) = delete;
    FrameBuf(FrameBuf &&rhs);
    FrameBuf &operator=(FrameBuf &&rhs);
};
