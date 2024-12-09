#include "AnimState.h"

Eng::AnimState::AnimState() {
    matr_palette_curr = std::make_unique<Ren::Mat4f[]>(256);
    matr_palette_prev = std::make_unique<Ren::Mat4f[]>(256);
    shape_palette_curr = std::make_unique<uint16_t[]>(256);
    shape_palette_prev = std::make_unique<uint16_t[]>(256);
}

void Eng::AnimState::Read(const Sys::JsObjectP &js_in, AnimState &as) {}