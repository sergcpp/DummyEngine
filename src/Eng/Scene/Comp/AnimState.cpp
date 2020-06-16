#include "AnimState.h"

AnimState::AnimState() {
    matr_palette_curr.reset(new Ren::Mat4f[256]);
    matr_palette_prev.reset(new Ren::Mat4f[256]);
    shape_palette_curr.reset(new uint16_t[256]);
    shape_palette_prev.reset(new uint16_t[256]);
}

void AnimState::Read(const JsObject &js_in, AnimState &as) {}