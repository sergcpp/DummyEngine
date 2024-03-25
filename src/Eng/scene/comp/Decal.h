#pragma once

#include <string_view>

#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
struct Decal {
    Ren::Mat4f view, proj;
    Ren::Vec4f mask, diff, norm, spec;

    static void Read(const JsObjectP &js_in, Decal &de);
    static void Write(const Decal &de, JsObjectP &js_out);

    static std::string_view name() { return "decal"; }
};
}
