#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

struct Drawable {
    enum eDrFlags {
        DrVisibleToShadow   = (1 << 0),
        DrMaterialOverride  = (1 << 1)
    };

    uint32_t            flags = 0;
    Ren::MeshRef        mesh;
    Ren::String         mesh_file;

    static void Read(const JsObject &js_in, Drawable &dr);
    static void Write(const Drawable &dr, JsObject &js_out);

    static const char *name() { return "drawable"; }
};