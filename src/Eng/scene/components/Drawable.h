#pragma once

#include <vector>

#include <Ren/Fwd.h>
#include <Ren/MVec.h>

#include "Common.h"

namespace Eng {
struct Drawable {
    enum class eVisibility : uint8_t { Camera, Shadow, Probes };
    static const Ren::Bitmask<eVisibility> DefaultVisMask;

    Ren::Bitmask<eVisibility> vis_mask = DefaultVisMask;
    Ren::MeshRef mesh;
    std::vector<std::array<Ren::MaterialRef, 3>> material_override;

    // TODO: allocate this dynamically (from pool)
    /*struct Ellipsoid {
        Ren::Vec3f offset;
        float radius;
        Ren::Vec3f axis;
        uint32_t bone_index;
        Ren::String bone_name;
    } ellipsoids[16];
    int ellipsoids_count = 0;*/

    static void Read(const Sys::JsObjectP &js_in, Drawable &dr);
    static void Write(const Drawable &dr, Sys::JsObjectP &js_out);

    static std::string_view name() { return "drawable"; }
};
}
