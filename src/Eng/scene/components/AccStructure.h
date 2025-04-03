#pragma once

#include <vector>

#include <Ren/Fwd.h>

#include "Common.h"

namespace Eng {
struct AccStructure {
    enum class eRayType : uint8_t { Camera, Diffuse, Specular, Refraction, Shadow, Volume };
    static const Ren::Bitmask<eRayType> DefaultVisMask;

    Ren::Bitmask<eRayType> vis_mask = DefaultVisMask;
    Ren::MeshRef mesh;
    std::vector<std::array<Ren::MaterialRef, 3>> material_override;
    float surf_area = 0.0f;

    static void Read(const Sys::JsObjectP &js_in, AccStructure &acc);
    static void Write(const AccStructure &acc, Sys::JsObjectP &js_out);

    static std::string_view name() { return "acc_structure"; }
};
} // namespace Eng
