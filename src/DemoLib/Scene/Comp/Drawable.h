#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

struct Drawable : public ComponentBase {
    Ren::MeshRef mesh;

    void Read(const JsObject &js_in) override {}
    void Write(JsObject &js_out) override {}
};