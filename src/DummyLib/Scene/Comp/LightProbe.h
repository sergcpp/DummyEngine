#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct LightProbe {
    int layer_index;
    float radius = 0.0f;
    Ren::Vec3f offset, sh_coeffs[4];

    static void Read(const JsObject &js_in, LightProbe &pr);
    static void Write(const LightProbe &pr, JsObject &js_out);

    static const char *name() { return "probe"; }
};