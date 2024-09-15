#pragma once

#include <cstdint>

#include "Keycode.h"
#include "MVec.h"

namespace Gui {
enum class eInputEvent : int16_t {
    None = -1,
    P1Down,
    P1Up,
    P1Move,
    P2Down,
    P2Up,
    P2Move,
    KeyDown,
    KeyUp,
    MouseWheel,
    Resize,
    _Count
};

struct input_event_t {
    eInputEvent type = eInputEvent::None;
    uint32_t key_code;
    Vec2f point;
    Vec2f move;
    uint64_t time_stamp;
};
}