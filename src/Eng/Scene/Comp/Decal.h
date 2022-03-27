#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct Decal {
    Ren::Mat4f view, proj;
    Ren::Vec4f mask, diff, norm, spec;

    static void Read(const JsObjectP &js_in, Decal &de);
    static void Write(const Decal &de, JsObjectP &js_out);

    static const char *name() { return "decal"; }
};