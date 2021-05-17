#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct LightProbe {
    // streaming data
    float radius = 0.0f;
    Ren::Vec3f offset, sh_coeffs[4];
    // temporary data
    int layer_index;

    static void Read(const JsObjectP &js_in, LightProbe &pr);
    static void Write(const LightProbe &pr, JsObjectP &js_out);

    static const char *name() { return "probe"; }
};