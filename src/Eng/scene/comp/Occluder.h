#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

namespace Eng {
struct Occluder {
    Ren::MeshRef mesh;

    static void Read(const JsObjectP &js_in, Occluder &occ) {}
    static void Write(const Occluder &occ, JsObjectP &js_out) {}

    static std::string_view name() { return "occluder"; }
};
} // namespace Eng