#pragma once

#include <string_view>

#include <Phy/Body.h>

#include "Common.h"

namespace Eng {
struct Physics {
    Phy::Body body;

    static void Read(const JsObjectP &js_in, Physics &ph);
    static void Write(const Physics &ph, JsObjectP &js_out);

    static std::string_view name() { return "physics"; }
};
} // namespace Eng