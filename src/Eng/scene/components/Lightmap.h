#pragma once

#include <string_view>

#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
struct Lightmap {
    // position and size in pixels of lightmap region on atlas
    int pos[2], size[2];
    // normalized position and size of lightmap region
    Ren::Vec4f xform;

    static void Read(const Sys::JsObjectP &js_in, Lightmap &lm);
    static void Write(const Lightmap &lm, Sys::JsObjectP &js_out);

    static std::string_view name() { return "lightmap"; }
};
} // namespace Eng
