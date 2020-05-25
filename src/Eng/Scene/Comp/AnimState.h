#pragma once

#include "Common.h"

class AnimState {
public:
    float anim_time_s = 0.0f;
    // TODO: allocate these from pool and of right length
    std::unique_ptr<Ren::Mat4f[]> matr_palette_curr, matr_palette_prev;

    AnimState();

    static void Read(const JsObject &js_in, AnimState &as);
    static void Write(const AnimState &as, JsObject &js_out) {}

    static const char *name() { return "anim_state"; }
};