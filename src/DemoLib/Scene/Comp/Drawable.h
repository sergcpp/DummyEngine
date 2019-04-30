#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

struct Drawable {
    Ren::MeshRef mesh;

    static void Read(const JsObject &js_in, Drawable &dr) {}
    static void Write(const Drawable &dr, JsObject &js_out) {}

    static const char *name() { return "drawable"; }
};