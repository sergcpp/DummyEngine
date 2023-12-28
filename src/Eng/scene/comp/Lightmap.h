#pragma once

#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
struct Lightmap {
    // position and size in pixels of lightmap region on atlas
    int pos[2], size[2];
    // normalized position and size of lightmap region
    Ren::Vec4f xform;

    static void Read(const JsObjectP &js_in, Lightmap &lm);
    static void Write(const Lightmap &lm, JsObjectP &js_out);

    static const char *name() { return "lightmap"; }
};
} // namespace Eng
