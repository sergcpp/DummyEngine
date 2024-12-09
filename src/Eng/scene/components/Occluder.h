#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

namespace Eng {
struct Occluder {
    Ren::MeshRef mesh;

    static void Read(const Sys::JsObjectP &js_in, Occluder &occ) {}
    static void Write(const Occluder &occ, Sys::JsObjectP &js_out) {}

    static std::string_view name() { return "occluder"; }
};
} // namespace Eng