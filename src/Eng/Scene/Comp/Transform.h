#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct Transform {
    // streaming data

    // temporary data
    Ren::Mat4f world_from_object, object_from_world, world_from_object_prev;
    Ren::Vec3f  bbox_min;
    uint32_t    node_index = 0xffffffff;
    Ren::Vec3f  bbox_max;
    Ren::Vec3f  bbox_min_ws, bbox_max_ws;
    Ren::Vec3f  euler_angles_rad, scale;
    uint32_t    pt_mi;

    void UpdateBBox();
    void UpdateInvMatrix();

    void UpdateTemporaryData() {
        UpdateBBox();
        UpdateInvMatrix();
    }

    static void Read(const JsObjectP &js_in, Transform &tr);
    static void Write(const Transform &tr, JsObjectP &js_out);

    static const char *name() { return "transform"; }
};