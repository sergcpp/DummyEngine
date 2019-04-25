#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct Decal : public ComponentBase {
    Ren::Mat4f view, proj;
    Ren::Vec4f diff, norm, spec;

    void Read(const JsObject &js_in) override;
    void Write(JsObject &js_out) override;
};