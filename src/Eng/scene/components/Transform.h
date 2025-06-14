#pragma once

#include <string_view>

#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
struct Transform {
    // streaming data
    Ren::Vec3d position;
    Ren::Vec3f euler_angles_rad, scale;

    // temporary data
    Ren::Mat4f world_from_object, object_from_world, world_from_object_prev;
    Ren::Vec3f bbox_min;
    uint32_t node_index = 0xffffffff;
    Ren::Vec3f bbox_max;
    Ren::Vec3f bbox_min_ws, bbox_max_ws;

    void UpdateBBox();
    void UpdateInvMatrix();

    void UpdateTemporaryData() {
        UpdateBBox();
        UpdateInvMatrix();
    }

    static void Read(const Sys::JsObjectP &js_in, Transform &tr);
    static void Write(const Transform &tr, Sys::JsObjectP &js_out);

    static std::string_view name() { return "transform"; }
};
} // namespace Eng
