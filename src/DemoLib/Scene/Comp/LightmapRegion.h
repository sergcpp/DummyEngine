#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct LightmapRegion : public ComponentBase {
    int pos[2], size[2];
    Ren::Vec4f xform;

    void Read(const JsObject &js_in) override {}
    void Write(JsObject &js_out) override {}
};