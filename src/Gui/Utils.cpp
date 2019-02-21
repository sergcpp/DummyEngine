#include "Utils.h"

Ren::Vec2f Gui::MapPointToScreen(const Vec2i &p, const Vec2i &res) {
    return (2.0f * Vec2f((float)p[0], (float)res[1] - p[1])) / (Vec2f)res + Vec2f(-1, -1);
}

