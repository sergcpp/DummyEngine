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

    static void Read(const JsObject &js_in, Drawable &dr);
    static void Write(const Drawable &dr, JsObject &js_out);

    static const char *name() { return "drawable"; }
};