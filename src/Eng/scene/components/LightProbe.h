#pragma once

#include <string_view>

#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
struct LightProbe {
    // streaming data
    float radius = 0.0f;
    Ren::Vec3f offset, sh_coeffs[4];
    // temporary data
    int layer_index;

    static void Read(const Sys::JsObjectP &js_in, LightProbe &pr);
    static void Write(const LightProbe &pr, Sys::JsObjectP &js_out);

    static std::string_view name() { return "probe"; }
};
} // namespace Eng
