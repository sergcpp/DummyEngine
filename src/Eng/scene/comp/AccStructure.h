#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

namespace Eng {
struct AccStructure {
    Ren::MeshRef mesh;
    float surf_area = 0.0f;

    static void Read(const JsObjectP &js_in, AccStructure &acc);
    static void Write(const AccStructure &acc, JsObjectP &js_out);

    static std::string_view name() { return "acc_structure"; }
};
} // namespace Eng
