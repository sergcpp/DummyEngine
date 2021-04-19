#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct Transform {
    // streaming data

    // temporary data
    Ren::Mat4f world_from_object, object_from_world, world_from_object_prev;
    Ren::Vec3f  bbox_min;
    uint32_t    node_index;
    Ren::Vec3f  bbox_max;
    Ren::Vec3f  bbox_min_ws, bbox_max_ws;
    Ren::Vec3f  euler_angles_rad;
    uint32_t    pt_mi;

    void UpdateBBox();
    void UpdateInvMatrix();

    void UpdateTemporaryData() {
        UpdateBBox();
        UpdateInvMatrix();
    }

    static void Read(const JsObject &js_in, Transform &tr);
    static void Write(const Transform &tr, JsObject &js_out);

    static const char *name() { return "transform"; }
};