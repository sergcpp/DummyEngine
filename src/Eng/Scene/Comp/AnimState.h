#pragma once

#include <memory>

#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
class AnimState {
  public:
    float anim_time_s = 0.0f;
    // TODO: allocate these from pool and of right length
    std::unique_ptr<Ren::Mat4f[]> matr_palette_curr, matr_palette_prev;
    std::unique_ptr<uint16_t[]> shape_palette_curr, shape_palette_prev;
    int shape_palette_count_curr = 0, shape_palette_count_prev = 0;

    AnimState();

    static void Read(const JsObjectP &js_in, AnimState &as);
    static void Write(const AnimState &as, JsObjectP &js_out) {}

    static const char *name() { return "anim_state"; }
};
} // namespace Eng