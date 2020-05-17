#pragma once

#include "Common.h"

class AnimState {
public:
    float anim_time_s = 0.0f;
    // TODO: allocate this dynamically (from pool)
    Ren::Mat4f matr_palette[256];

    static void Read(const JsObject &js_in, AnimState &as);
    static void Write(const AnimState &as, JsObject &js_out) {}

    static const char *name() { return "anim_state"; }
};