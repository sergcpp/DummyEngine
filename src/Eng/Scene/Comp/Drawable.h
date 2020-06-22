#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

struct Drawable {
    enum class eDrFlags {
        DrMaterialOverride  = (1 << 0)
    };

    enum class eDrVisibility {
        VisShadow = (1 << 0),
        VisProbes = (1 << 1)
    };

    uint32_t            flags = 0, vis_mask = 0xffffffff;
    Ren::MeshRef        mesh, pt_mesh;
    Ren::String         mesh_file;

    // TODO: allocate this dynamically (from pool)
    struct Ellipsoid {
        Ren::Vec3f offset;
        float radius;
        Ren::Vec3f axis;
        uint32_t bone_index;
        Ren::String bone_name;
    } ellipsoids[16];
    int ellipsoids_count = 0;

    static void Read(const JsObject &js_in, Drawable &dr);
    static void Write(const Drawable &dr, JsObject &js_out);

    static const char *name() { return "drawable"; }
};
