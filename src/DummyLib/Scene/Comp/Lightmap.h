#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct Lightmap {
    // position and size in pixels of lightmap region on atlas
    int pos[2], size[2];
    // normalized position and size of lightmap region
    Ren::Vec4f xform;

    static void Read(const JsObject &js_in, Lightmap &lm);
    static void Write(const Lightmap &lm, JsObject &js_out) {}

    static const char *name() { return "lightmap"; }
};