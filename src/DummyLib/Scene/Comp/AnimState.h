#pragma once

#include "Common.h"

class AnimState {
public:
    float anim_time_s = 0.0f;
    Ren::Mat4f matr_palette[64];

    static void Read(const JsObject &js_in, AnimState &as);
    static void Write(const AnimState &as, JsObject &js_out) {}

    static const char *name() { return "anim_state"; }
};