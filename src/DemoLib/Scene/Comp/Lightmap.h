#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct Lightmap {
    int pos[2], size[2];
    Ren::Vec4f xform;
    uint32_t pt_mi;

    static void Read(const JsObject &js_in, Lightmap &lm);
    static void Write(const Lightmap &lm, JsObject &js_out) {}

    static const char *name() { return "lightmap"; }
};