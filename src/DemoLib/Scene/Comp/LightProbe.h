#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct LightProbe : public ComponentBase {
    int layer_index;
    float radius = 0.0f;
    Ren::Vec3f offset, sh_coeffs[4];

    void Read(const JsObject &js_in) override;
    void Write(JsObject &js_out) override;
};